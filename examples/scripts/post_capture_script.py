#!/usr/bin/env python3

import sys
import json
import os

def log_message(message):
    """Logs a message to a file in the user's home directory."""
    log_file = os.path.expanduser("~/post_capture_log.txt")
    with open(log_file, "a") as f:
        f.write(f"{message}\n")

def main():
    """
    Parses JSON from stdin and logs relevant information from a post-capture event.
    """
    try:
        # Read the JSON data from standard input
        input_data = sys.stdin.read()
        if not input_data:
            log_message("No data received from stdin.")
            return

        data = json.loads(input_data)

        script_type = data.get("script_type", "UNKNOWN")
        log_message(f"Script Type: {script_type}")

        if script_type == "POST_CAPTURE":
            # Log FITS info if available
            fits_info = data.get("fits")
            if fits_info:
                filename = fits_info.get("filename", "N/A")
                log_message(f"FITS Filename: {filename}")

                headers = fits_info.get("headers")
                if headers:
                    object_name = headers.get("OBJECT", "Unknown Object")
                    log_message(f"Captured Object: {object_name}")
                    log_message("--- FITS Headers ---")
                    for key, value in headers.items():
                        log_message(f"{key}: {value}")
                    log_message("--------------------")

            # Log job progress
            progress_info = data.get("progress")
            if progress_info:
                completed = progress_info.get("completed", 0)
                total = progress_info.get("total", 0)
                log_message(f"Capture Progress: {completed}/{total}")

    except json.JSONDecodeError:
        log_message(f"Failed to decode JSON: {input_data}")
    except Exception as e:
        log_message(f"An unexpected error occurred: {e}")

if __name__ == "__main__":
    main()
