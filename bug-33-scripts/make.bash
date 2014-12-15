if [ ! -d percona-server-5.6 ] ; then git clone git@github.com:Tokutek/percona-server-5.6; fi
if [ ! -d tokudb-engine ] ; then git clone git@github.com:Tokutek/tokudb-engine; fi
if [ ! -d ft-index ] ; then git clone git@github.com:kuszmaul/ft-index; fi

pushd tokudb-engine/storage
cp -r tokudb ../../percona-server-5.6/storage
popd

pushd percona-server-5.6/storage/tokudb
ln -s ../../../ft-index
popd

mkdir build install
pushd build
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=../install ../percona-server-5.6
make -j8 install
popd

pushd install
scripts/mysql_install_db --defaults-file=~/rfp.cnf
popd