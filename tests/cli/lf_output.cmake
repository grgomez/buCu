# Runs the built bucu with standard output and standard error redirected to files, so no console
# is involved, and fails when a file holds a carriage return (docs/product.md section 5: every
# output line ends in LF on every platform). In-process tests write to string streams and cannot
# see the C runtime's newline translation; this one runs the real program.
#
# Usage: cmake -DBUCU_EXE=<bucu> -DWORK_DIR=<directory> -P lf_output.cmake

if(NOT BUCU_EXE OR NOT WORK_DIR)
    message(FATAL_ERROR "lf_output.cmake needs -DBUCU_EXE and -DWORK_DIR")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
set(DB "${WORK_DIR}/lf.db")

# run(<name> <expected exit code> <bucu arguments...>) writes <name>.out and <name>.err.
function(run name expected_code)
    execute_process(
        COMMAND "${BUCU_EXE}" --db "${DB}" ${ARGN}
        RESULT_VARIABLE code
        OUTPUT_FILE "${WORK_DIR}/${name}.out"
        ERROR_FILE "${WORK_DIR}/${name}.err")
    if(NOT code STREQUAL expected_code)
        message(FATAL_ERROR "bucu ${ARGN} exited ${code}, expected ${expected_code}")
    endif()
endfunction()

# Fails unless the file has at least one line and no CR byte. Compares whole bytes, so a CR cannot
# be matched across the boundary of two bytes.
function(check_lf file)
    file(READ "${file}" hex HEX)
    string(REGEX MATCHALL "[0-9a-f][0-9a-f]" bytes "${hex}")
    if(NOT bytes)
        message(FATAL_ERROR "${file} is empty; a line was expected")
    endif()
    list(FIND bytes "0d" carriage_return)
    if(NOT carriage_return EQUAL -1)
        message(FATAL_ERROR "${file} holds a carriage return at byte ${carriage_return}")
    endif()
    list(FIND bytes "0a" line_feed)
    if(line_feed EQUAL -1)
        message(FATAL_ERROR "${file} holds no line feed")
    endif()
endfunction()

run(add 0 add https://example.org web dev --title "Example" --description "one\ntwo")
check_lf("${WORK_DIR}/add.out")

run(version 0 db version)
check_lf("${WORK_DIR}/version.out")

run(path 0 db path)
check_lf("${WORK_DIR}/path.out")

run(check 0 db check)
check_lf("${WORK_DIR}/check.out")

run(backup 0 db backup "${WORK_DIR}/copy.db")
check_lf("${WORK_DIR}/backup.out")

run(invalid 3 add example.org)
check_lf("${WORK_DIR}/invalid.err")

run(duplicate 4 add https://example.org)
check_lf("${WORK_DIR}/duplicate.err")

run(usage 2 add)
check_lf("${WORK_DIR}/usage.err")
