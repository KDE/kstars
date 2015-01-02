use 5.008008;
use ExtUtils::MakeMaker;

$CC = 'g++';

# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME              => 'HTMesh',
    VERSION_FROM      => 'lib/HTMesh.pm', # finds $VERSION
    PREREQ_PM         => {}, # e.g., Module::Name => 1.1
    ($] >= 5.005 ?     ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM  => 'lib/HTMesh.pm', # retrieve abstract from module
       AUTHOR         => 'James Bowlin <jbowlin@mindspring.com>') : ()),
    LIBS              => ['-lhtmesh'], # e.g., '-lm'
    DEFINE            => '', # e.g., '-DHAVE_SOMETHING'
    CC                => $CC,
    LD                => '$(CC)',
    INC               => '-I. -I../../../htmesh', 
	# Un-comment this if you add C files to link with later:
    # OBJECT            => '$(O_FILES)', # link all the C files too
    XSOPT             => '-C++',
    TYPEMAPS          => ['perlobject.map'],
);
