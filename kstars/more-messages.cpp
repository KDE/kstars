// Introduce some strings into the po file that are not yet used in
// trunk, but will be when OpenGL bugs are fixed is complete

// This is really critical for release, but nobody has time at the
// moment to fix the smooth switching between OpenGL and QPainter
// backends at run time, but will almost certainly have time before
// RC1 is tagged.

// This is a makeshift solution and this file is not (thankfully)
// compiled.

i18n("This version of KStars comes with new experimental OpenGL support. Our experience is that OpenGL works much faster on machines with hardware acceleration. Would you like to switch to OpenGL painting backends?");

i18n("Switch to OpenGL backend");

i18n("Switch to QPainter backend");

i18n("Infoboxes will be disabled as they do not work correctly when using OpenGL backends as of this version");

