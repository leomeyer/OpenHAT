#/bin/bash

TARFOLDER=openhatd-$1

echo Preparing tar folder $TARFOLDER...

mkdir -p $TARFOLDER
mkdir -p $TARFOLDER/bin
echo Copying main binary...
cp src/openhatd $TARFOLDER/bin

#ifeq ($(INCLUDE_POCO_SO),1)
#echo Copying POCO libraries...
#ifeq ($(DEBUG),1)
#cp $(POCOLIBPATH)/*d.so.$(POCO_LIBVERSION) $(TARFOLDER)/bin
#else
#cp $(POCOLIBPATH)/*.so.$(POCO_LIBVERSION) $(TARFOLDER)/bin
#find $(TARFOLDER)/bin/ -type f -name '*d.so.*' -exec rm {} \;		
#endif
#endif
cp ../src/hello-world.ini $TARFOLDER/bin/

echo Preparing plugin folder...
mkdir -p $TARFOLDER/plugins
find plugins/ -name '*.so' -exec cp --parents {} $TARFOLDER \;
cp -r ../plugins/WebServerPlugin/webdocroot $TARFOLDER/plugins/WebServerPlugin
echo Copying testconfigs...
cp -r ../testconfigs $TARFOLDER
#echo Preparing documentation folder...
#mkdir -p $(TARFOLDER)/doc
#	mkdir -p ../openhatd-docs-$(VERSION)
#	cp -r ../openhatd-docs-$(VERSION)/* $(TARFOLDER)/doc/
#find $(TARFOLDER)/doc/ -type f -name '*.vsdx' -exec rm {} \;

echo Making tar...
tar czf $TARFOLDER.tar.gz $TARFOLDER
#rm -rf $TARFOLDER
echo Calculating checksum...
md5sum $TARFOLDER.tar.gz > $TARFOLDER.tar.gz.md5
echo Done.
