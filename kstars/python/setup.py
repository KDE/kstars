import os
import sys
import pathlib
import subprocess
import shutil
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=""):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    def run(self):
        try:
            subprocess.check_output(["cmake", "--version"])
        except OSError:
            raise RuntimeError(
                "CMake must be installed to build the following extensions: "
                + ", ".join(e.name for e in self.extensions)
            )

        for ext in self.extensions:
            self.build_extension(ext)

    def build_extension(self, ext):
        extdir = os.path.dirname(self.get_ext_fullpath(ext.name))

        # required for auto-detection of auxiliary "native" libs
        if not extdir.endswith(os.path.sep):
            extdir += os.path.sep

        cmake_args = [
            "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=" + extdir,
            "-DPYTHON_EXECUTABLE=" + sys.executable,
        ]

        cfg = "Debug" if self.debug else "Release"
        build_args = ["--config", cfg]

        cmake_args += ["-DCMAKE_BUILD_TYPE=" + cfg, "-DBUILD_PYKSTARS=YES", "-GNinja"]
        build_args += ["--"]

        env = os.environ.copy()
        env["CXXFLAGS"] = '{} -DVERSION_INFO=\\"{}\\"'.format(
            env.get("CXXFLAGS", ""), self.distribution.get_version()
        )
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)

        if not os.path.exists(extdir):
            os.makedirs(extdir)

        subprocess.check_call(
            ["cmake", ext.sourcedir + "/../../"] + cmake_args,
            cwd=self.build_temp,
            env=env,
        )

        subprocess.check_call(
            ["cmake", "--build", ".", "--target", "pykstars"] + build_args,
            cwd=self.build_temp,
        )

        ext_file = subprocess.run(
            ["find", "kstars", "-name", "pykstars*.so"],
            cwd=self.build_temp,
            check=True,
            capture_output=True,
        ).stdout.decode("utf-8")[:-1]

        shutil.copy2(os.path.join(self.build_temp, ext_file), extdir)


# The directory containing this file
HERE = pathlib.Path(__file__).parent

# The text of the README file
README = (HERE / "readme.md").read_text()

setup(
    name="pykstars",
    version="0.0.1",
    author="Valentin Boettcher",
    author_email="hiro@protagon.space",
    description="Some python bindings for kstars. Primarily used to package DSO catalogs.",
    long_description_content_type="text/markdown",
    license="GPLv2+",
    long_description=README,
    include_package_data=True,
    ext_modules=[CMakeExtension("pykstars")],
    cmdclass=dict(build_ext=CMakeBuild),
    zip_safe=False,
)
