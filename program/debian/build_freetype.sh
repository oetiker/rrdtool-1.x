:
# Build freetype2 for rrdtool on Debian/Linux
# 12-May-2004 Mike Slifcak

FOUND=`find /lib /usr/lib /usr/local/lib -name libfreetype.a | wc -l`
FOUND=`echo $FOUND`
if [ $FOUND -lt 1 ] ; then
########################################
## Build the independent object freetype2
########################################
cd /tmp
rm -rf freetype*/
if [ ! -e freetype*gz ] ; then
  echo "get freetype-2.1.8 or stable from http://freetype.sf.net/"
  exit 1
fi
tar tzf freetype*gz > /dev/null 2>&1
RC=$?
if [ $RC -ne 0 ] ; then
  echo "Need one good freetype*gz.  Just one. In /tmp.  Thanks!"
  exit 1
fi

echo -n "Testing "
ls  freetype*gz
tar xzf freetype*gz
echo "Building freetype"
cd freetype-*/
./configure --disable-shared > cfg.out 2>&1
make > make.out 2>&1
make install > inst.out 2>&1
grep Error *.out
if [ $? -ne 1 ] ; then
   echo  "Building freetype failed. See `pwd`/*.out for details"
   exit 1
fi
cd ..
fi ## skip freetype build

echo "Building freetype succeeded."

exit 0 
