#!/bin/bash

set -euo pipefail

repo_root="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "${repo_root}"

options_file="${ASTYLE_OPTIONS_FILE:-.astylerc}"
version_file="${ASTYLE_VERSION_FILE:-.astyle-version}"
suffix="${ASTYLE_SUFFIX:-none}"

if [[ ! -f "${options_file}" ]]; then
    echo "ERROR: astyle options file not found: ${options_file}" >&2
    exit 1
fi

required_version="${ASTYLE_VERSION:-}"
if [[ -z "${required_version}" ]] && [[ -f "${version_file}" ]]; then
    required_version="$(<"${version_file}")"
    required_version="${required_version#"${required_version%%[![:space:]]*}"}"
    required_version="${required_version%"${required_version##*[![:space:]]}"}"
fi

declare -a astyle_cmd=()
if [[ -n "${ASTYLE_BIN:-}" ]]; then
    astyle_cmd=("${ASTYLE_BIN}")
elif [[ -n "${required_version}" ]] && command -v uvx >/dev/null 2>&1 && [[ "${ASTYLE_USE_SYSTEM:-0}" != "1" ]]; then
    astyle_cmd=(uvx --from "astyle==${required_version}" astyle)
elif command -v astyle >/dev/null 2>&1; then
    astyle_cmd=(astyle)
else
    echo "ERROR: 'astyle' not found. Install it, or set ASTYLE_BIN, or install uv/uvx and add .astyle-version." >&2
    exit 127
fi

if [[ -n "${required_version}" ]]; then
    version_output="$("${astyle_cmd[@]}" --version 2>/dev/null || true)"
    if ! printf '%s\n' "${version_output}" | grep -q "Version ${required_version}"; then
        echo "ERROR: astyle version mismatch. Expected ${required_version}, got:" >&2
        printf '%s\n' "${version_output:-<no output>}" >&2
        exit 1
    fi
fi

check_mode=0
changed_since=""

declare -a passthrough=()
declare -a paths=()

while (( $# > 0 )); do
    case "$1" in
        --check)
            check_mode=1
            shift
            ;;
        --changed-since)
            if (( $# < 2 )); then
                echo "ERROR: --changed-since requires a git ref/sha argument." >&2
                exit 2
            fi
            changed_since="$2"
            shift 2
            ;;
        --)
            shift
            paths+=("$@")
            break
            ;;
        -*)
            passthrough+=("$1")
            shift
            ;;
        *)
            paths+=("$1")
            shift
            ;;
    esac
done

declare -a targets=()

if (( ${#paths[@]} > 0 )); then
    targets=("${paths[@]}")
elif [[ -n "${changed_since}" ]]; then
    tmpfile="$(mktemp)"
    set +e
    git diff --name-only -z --diff-filter=ACMRTUXB "${changed_since}...HEAD" >"${tmpfile}" 2>/dev/null
    rc=$?
    set -e
    if (( rc != 0 )); then
        echo "WARN: failed to compute changed files from '${changed_since}...HEAD'; falling back to all tracked files." >&2
    else
        while IFS= read -r -d '' path; do
            targets+=("${path}")
        done < "${tmpfile}"
    fi
    rm -f "${tmpfile}"
else
    while IFS= read -r -d '' path; do
        targets+=("${path}")
    done < <(git ls-files -z)
fi

declare -a filtered=()
for path in "${targets[@]}"; do
    case "${path}" in
        *.c|*.cc|*.cpp|*.cxx|*.h|*.hh|*.hpp|*.hxx)
            if [[ -f "${path}" ]]; then
                filtered+=("${path}")
            fi
            ;;
        *)
            ;;
    esac
done

if (( ${#filtered[@]} == 0 )); then
    echo "No matching source files to format." >&2
    exit 0
fi

if (( check_mode == 1 )); then
    output="$(
        printf '%s\0' "${filtered[@]}" |
            xargs -0 "${astyle_cmd[@]}" --options="${options_file}" --suffix="${suffix}" --dry-run --formatted "${passthrough[@]}"
    )"
    if [[ -n "${output}" ]]; then
        echo "${output}"
        echo "ERROR: astyle would reformat files (see above). Run: bash tools/run_astyle.sh" >&2
        exit 1
    fi
    exit 0
fi

printf '%s\0' "${filtered[@]}" | xargs -0 "${astyle_cmd[@]}" --options="${options_file}" --suffix="${suffix}" "${passthrough[@]}"
