# F-13: frostbyte_sim --runs zweimal mit denselben Fixtures laufen lassen, die Ausgabe muss byte-gleich sein.
# Aufruf: cmake -DSIM=<frostbyte_sim> -DFIXTURES=<test/fixtures> -DOUT=<Arbeitsordner> -P cli_determinism.cmake
foreach(run a b)
    file(REMOVE_RECURSE "${OUT}/${run}")
    execute_process(
        COMMAND "${SIM}" --demo --runs 3 --seed 5 --out "${OUT}/${run}"
                --params "${FIXTURES}/sim_params_game.json" --field "${FIXTURES}/field_game.json"
        RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "frostbyte_sim fehlgeschlagen (${rc}): ${err}")
    endif()
endforeach()
file(GLOB files RELATIVE "${OUT}/a" "${OUT}/a/*.csv")
list(LENGTH files n)
if(NOT n EQUAL 6)
    message(FATAL_ERROR "erwartet 6 CSV-Dateien (3 Läufe x 2 Roboter), gefunden: ${n}")
endif()
foreach(f ${files})
    execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${OUT}/a/${f}" "${OUT}/b/${f}" RESULT_VARIABLE diff)
    if(NOT diff EQUAL 0)
        message(FATAL_ERROR "nicht byte-gleich: ${f}")
    endif()
endforeach()
