# sperf: perf record -q --call-graph lbr --switch-output -- "$@"
# ====== perfsvg: ======
# if [[ $# -eq 0 ]]; then
#   echo "  Usage: perfsvg <filename> [target file name]"
#   exit 0
# fi
# # get scripts
# ghroot="https://raw.githubusercontent.com/brendangregg/FlameGraph/master"
# bindir="${HOME}/program/usr/bin"
# st1="${bindir}/stackcollapse-perf.pl"
# st2="${bindir}/flamegraph.pl"
# if [[ ! -f "${st1}" ]]; then
#   mkdir -p ~/program/usr/bin
#   wget "${ghroot}/stackcollapse-perf.pl" -O "${st1}"
# fi
# if [[ ! -f "${st2}" ]]; then
#   mkdir -p ~/program/usr/bin
#   wget "${ghroot}/flamegraph.pl" -O "${st2}"
# fi

# rid=$(timestamp)
# pref="/tmp/.fperf-${rid}"
# perf script -i "${1}" > ${pref}.perf
# perl -w "${st1}" ${pref}.perf > ${pref}.folded
# if [[ $# -eq 2 ]]; then
#   perl -w "${st2}" ${pref}.folded > ${2}.svg
#   echo "${2}.svg"
# else
#   perl -w "${st2}" ${pref}.folded > fperf-${rid}.svg
#   echo "fperf-${rid}.svg"
# fi
# rm -f ${pref}.perf ${pref}.folded
#  =======================
rm -rf perf.data*
sperf ../debug/hash_bench --thread_read=1

declare -i x=0
for f in perf.data.*;
do
    if [[ "$x" == 1 ]]; then
        # probe Put
        echo "processing $f";
        perfsvg $f "put"
    fi

    if [[ "$x" == 3 ]]; then
        # probe Get
        echo "processing $f";
        perfsvg $f "get"
    fi
    x=$((x + 1))
    # echo $x
done