#!/bin/bash

# What this is:
#
# Helpful script to download Digitized Sky Survey images using KStars
# DBus interfaces to resolve object names. The script is designed to
# produce fixed FOVs instead of relying on KStars' object-specific FOV
# choices.

# Data use and copyrights:
#
# Please note that the images downloaded are governed by the terms of
# the data use policies of the Digital Sky Survey. Relevant links are:
# http://archive.stsci.edu/dss/acknowledging.html
# http://archive.stsci.edu/data_use.html
#
# SPDX-FileCopyrightText: 2015 Akarsh Simha <akarsh@kde.org>
# SPDX-License-Identifier: GPL-3.0-or-later

# Dependencies:
#
# Requires: qdbus, wget
# Optional: ImageMagick suite (for mogrify)


# Usage:
#
# ./dss_downloader.sh <list_file>
#
# list_file is a text file containing a list of objects known to
# KStars. One object on one line
#
# You may need to comment / uncomment some lines and/or modify some
# numbers to suit your needs

# Specify the sizes of the images below in arcminutes. No
# minimum. Maximum 75 arcminutes.
size_1="15"
size_2="30"
#size_3="45"

# Read the file
while read object; do
    echo "========== Processing ${object} =========="

    # Get the DSS URL from KStars
    raw_dss_url=$(qdbus org.kde.kstars /KStars org.kde.kstars.getDSSURL "${object}")

    # Strip height and width from URL
    trimmed_dss_url=$(echo ${raw_dss_url} | sed 's/&h=[0-9\.]*&w=[0-9\.]*//')

    # Prepare URL for each size
    dss_url_size1="${trimmed_dss_url}&h=${size_1}&w=${size_1}"
    dss_url_size2="${trimmed_dss_url}&h=${size_2}&w=${size_2}"
#    dss_url_size3="${trimmed_dss_url}&h=${size_3}&w=${size_3}"

    # Prepare file names for each size
    object_underscored=$(echo ${object} | sed 's/  */_/g')
    filename_size1="${object_underscored}_${size_1}x${size_1}.gif"
    filename_size2="${object_underscored}_${size_2}x${size_2}.gif"
#    filename_size3="${object_underscored}_${size_3}x${size_3}.gif"

    # Download the images
    wget ${dss_url_size1} -O ${filename_size1}
    wget ${dss_url_size2} -O ${filename_size2}
#    wget ${dss_url_size3} -O ${filename_size3}

    # Optional -- comment to disable
    # Make the DSS images white on black instead of black on white
#    mogrify -negate ${filename_size1}
#    mogrify -negate ${filename_size2}
#    mogrify -negate ${filename_size3}

    # Optional -- uncomment to enable
    # Scale down images that are larger than 1024x1024 pixels in size to 1024x1024
    # mogrify ${filename_size1} -resize "1024>"
    # mogrify ${filename_size2} -resize "1024>"
    # mogrify ${filename_size3} -resize "1024>"

done < $1;

echo "========== ALL DONE! =========="

