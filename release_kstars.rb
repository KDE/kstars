#!/usr/bin/env ruby1.8
#
# Ruby script for generating KStars tarball releases from SVN
#
# (c) 2005 Mark Kretschmann <markey@web.de>
# modified by Carsten Niehaus
# Some parts of this code taken from cvs2dist
# License: GPL V2

doi18n = "0"
dogpg = "0"

unless $*.empty?()
    case $*[0]
        when "--nogpg"
			dogpg = "1"
        when "--noi18n"
			doi18n = "1"
        when "--help"
			puts "use --noi18n or --nogpg"
			exit
        else
            puts("Unknown option #{$1}. Use --noi18n or --nogpg.\n")
			exit
    end
    case $*[1]
        when "--nogpg"
			dogpg = "1"
        when "--noi18n"
			doi18n = "1"
        when "--help"
			puts "use --noi18n or --nogpg"
			exit
        else
            puts("Unknown option #{$1}. Use --noi18n or --nogpg.\n")
			exit
    end
end

puts doi18n
puts dogpg

#Ask user for app version and name
version  = `kdialog --inputbox "Name"`.chomp
name     = "kstars" #`kdialog --inputbox "Versionnumber: "`.chomp

folder   = "#{name}-#{version}"

# Some helper methods
def svn( command, dir )
    `svn #{command} svn://anonsvn.kde.org/home/kde/#{dir}`
end

def svnup( dir )
    `svn up #{dir}`
end


# Prevent using unsermake
oldmake = ENV["UNSERMAKE"]
ENV["UNSERMAKE"] = "no"

# Remove old folder, if exists
`rm -rf #{folder} 2> /dev/null`
`rm -rf folder.tar.bz2 2> /dev/null`

Dir.mkdir( folder )
Dir.chdir( folder )

puts "Entering "+`pwd`

svn( "co -N", "/trunk/KDE/kdeedu/" )
Dir.chdir( "kdeedu")

puts "Checking out libkdeedu"
svnup("libkdeedu")
puts "Checking out #{name}"
svnup("#{name}")
svn( "co", "/trunk/KDE/kde-common/admin")

# we check out kde-i18n/subdirs in kde-l10n..
puts doi18n
if doi18n == "0"
    puts "\n"
    puts "**** l10n ****"
    puts "\n"
	
	Dir.mkdir( "doc" )

    i18nlangs = `svn cat https://svn.kde.org/home/kde/trunk/l10n/subdirs`
    Dir.mkdir( "l10n" )
    Dir.chdir( "l10n" )

    # docs
    for lang in i18nlangs
        lang.chomp!
        `rm -rf ../doc/#{lang}`
        `rm -rf #{name}`
        docdirname = "l10n/#{lang}/docs/kdeedu/#{name}"
        `svn co -q https://svn.kde.org/home/kde/trunk/#{docdirname} > /dev/null 2>&1`
        next unless FileTest.exists?( "#{name}" )
		`cp -R #{name}/ ../doc/#{lang}`

        # we don't want KDE_DOCS = AUTO, cause that makes the
        # build system assume that the name of the app is the
        # same as the name of the dir the Makefile.am is in.
        # Instead, we explicitly pass the name..
		makefile = File.new( "../doc/#{lang}/Makefile.am", File::CREAT | File::RDWR | File::TRUNC )
		makefile << "KDE_LANG = #{lang}\n"
		makefile << "KDE_DOCS = #{name}\n"
		makefile.close()

        puts( "#{lang} done.\n" )
    end
	
	#now create the Makefile.am so that the docs will be build
	makefile = File.new( "../doc/Makefile.am", File::CREAT | File::RDWR | File::TRUNC )
	makefile << "KDE_LANG = en\n"
	makefile << "KDE_DOCS = AUTO\n"
	makefile << "SUBDIRS = $(AUTODIRS)\n"
	makefile.close()

    Dir.chdir( ".." ) # multimedia
    puts "\n"

    $subdirs = false
    Dir.mkdir( "po" )

    for lang in i18nlangs
        lang.chomp!
        pofilename = "l10n/#{lang}/messages/kdeedu/#{name}.po"
        `svn cat https://svn.kde.org/home/kde/trunk/#{pofilename} 2> /dev/null | tee l10n/#{name}.po`
        next if FileTest.size( "l10n/#{name}.po" ) == 0

        dest = "po/#{lang}"
        Dir.mkdir( dest )
        print "Copying #{lang}'s #{name}.po over ..  "
        `mv l10n/#{name}.po #{dest}`
        puts( "done.\n" )

        makefile = File.new( "#{dest}/Makefile.am", File::CREAT | File::RDWR | File::TRUNC )
        makefile << "KDE_LANG = #{lang}\n"
        makefile << "SUBDIRS  = $(AUTODIRS)\n"
        makefile << "POFILES  = AUTO\n"
        makefile.close()

        $subdirs = true
    end

    if $subdirs
        makefile = File.new( "po/Makefile.am", File::CREAT | File::RDWR | File::TRUNC )
        makefile << "SUBDIRS = $(AUTODIRS)\n"
        makefile.close()
    else
		puts "Removing po-subdirectory"
        `rm -Rf po`
    end

    `rm -rf l10n`
end

puts "\n"

puts "Removing svn-history files (almost 10 megabyte)"
`find -name ".svn" | xargs rm -rf`

Dir.chdir( "#{name}" )

`rm -rf debian`

Dir.chdir( ".." ) # kdeedu 
puts( "\n" )

`find | xargs touch`


puts "**** Generating Makefiles..  "
`make -f Makefile.cvs`
puts "done.\n"

`rm -rf autom4te.cache`
`rm stamp-h.in`


puts "**** Compressing..  "
`mv * ..`
Dir.chdir( ".." ) 
`rm -rf kde-common`
`rm -rf kdeedu` # after the moving of the directory this is empty
Dir.chdir( ".." ) # root folder
`tar -cf #{folder}.tar #{folder}`
`bzip2 #{folder}.tar`
`rm -rf #{folder}`
puts "done.\n"


ENV["UNSERMAKE"] = oldmake

if dogpg == "0"
	`gpg --detach-sign #{folder}.tar.bz2`
end


puts "\n"
puts "====================================================="
puts "Congratulations :) #{name} #{version} tarball generated.\n"
puts "\n"
puts "MD5 checksum: " + `md5sum #{folder}.tar.bz2`
if dogpg == "0"
	puts "The user can verify this package with "
	puts "\n"
	puts "gpg --verify #{folder}.tar.bz2.sig #{folder}.tar.bz2"
end
puts "\n"
puts "\n"


