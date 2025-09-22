program_name="CBuilder"
build_dir="./.Build"

sources="./Src/Main.c"
debug_opts="-O0 -g -DDEBUG"
release_opts="-O2 -DRELEASE"
comp_opts="-Wall -Wextra -std=c17 -march=x86-64-v3"
link_opts="-lc -ldl -lm -lpthread"

if [ "$1" == "debug" ]; then
    echo "--- Building in debug mode ---"
    comp_opts="$comp_opts $debug_opts"
elif [ "$1" == "release" ]; then
    echo "--- Building in release mode ---"
    comp_opts="$comp_opts $release_opts"
elif [ "$1" == "run" ]; then
    echo "--- Running $program_name ---"
    $build_dir/$program_name $2
    exit $?
else
    echo "Invalid command. Use 'debug', 'release' or 'run'"
    exit -1
fi

if [ ! -d "$build_dir" ]; then
    mkdir $build_dir
fi

echo "--- Building $program_name ---"
echo "$comp_opts $sources $link_opts"
musl-gcc $comp_opts $sources $link_opts -o $build_dir/$program_name

exit $?
