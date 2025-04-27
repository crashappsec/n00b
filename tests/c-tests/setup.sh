if [[ -f __cur.c ]] ; then
    rm __cur.c
fi
cp $@ __cur.c
cd ../../
dev build ctest
rm tests/c-tests/__cur.c
export CMD=build_`cat .meson_last`
$CMD/ctest
