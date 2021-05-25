#/bin/bash

if [ "$1" = "" ]; then
    echo "No release suffix specified; do not call this script directly!"
    echo "It is called by 'make release' in the cmake-generated build folder."
    exit
fi


TARFOLDER=openhatd-$1

echo Preparing tar folder $TARFOLDER...

mkdir -p $TARFOLDER
mkdir -p $TARFOLDER/bin
echo Copying main binary...
cp src/openhatd $TARFOLDER/bin

cp ../src/hello-world.ini $TARFOLDER/bin/

echo Preparing plugin folder...
mkdir -p $TARFOLDER/plugins
find plugins/ -name '*.so' -exec cp --parents {} $TARFOLDER \;
cp -r ../plugins/WebServerPlugin/webdocroot $TARFOLDER/plugins/WebServerPlugin
echo Copying testconfigs...
cp -r ../testconfigs $TARFOLDER

echo Making tar...
tar czf $TARFOLDER.tar.gz $TARFOLDER
rm -rf $TARFOLDER
echo Calculating checksum...
md5sum $TARFOLDER.tar.gz > $TARFOLDER.tar.gz.md5
echo Done.
