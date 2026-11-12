# F-13: frostbyte_sim --runs zweimal mit denselben Fixtures laufen lassen, die Ausgabe muss byte-gleich sein.
# Aufruf: cmake -DSIM=<frostbyte_sim> -DFIXTURES=<test/fixtures> -DOUT=<Arbeitsordner> -P cli_determinism.cmake
# Zwei Wege: Demo (Spannungsfolgen) und Routen mit Regelkreis und Rauschen (der Weg, auf dem Nichtdeterminismus entstünde).
function(check_same name)
    foreach(run a b)
        file(REMOVE_RECURSE "${OUT}/${name}_${run}")
        execute_process(
            COMMAND "${SIM}" ${ARGN} --runs 3 --seed 5 --out "${OUT}/${name}_${run}"
            RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err)
        if(NOT rc EQUAL 0)
            message(FATAL_ERROR "${name}: frostbyte_sim fehlgeschlagen (${rc}): ${err}")
        endif()
    endforeach()
    file(GLOB files RELATIVE "${OUT}/${name}_a" "${OUT}/${name}_a/*.csv")
    list(LENGTH files n)
    if(NOT n EQUAL 6)
        message(FATAL_ERROR "${name}: erwartet 6 CSV-Dateien (3 Läufe x 2 Roboter), gefunden: ${n}")
    endif()
    foreach(f ${files})
        execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${OUT}/${name}_a/${f}" "${OUT}/${name}_b/${f}"
                        RESULT_VARIABLE diff)
        if(NOT diff EQUAL 0)
            message(FATAL_ERROR "${name}: nicht byte-gleich: ${f}")
        endif()
    endforeach()
endfunction()

check_same(demo --demo --params "${FIXTURES}/sim_params_game.json" --field "${FIXTURES}/field_game.json")
check_same(route --stacker "${FIXTURES}/route_meet_stacker.route" --runner "${FIXTURES}/route_meet_runner.route"
           --params "${FIXTURES}/sim_params_min.json" --field "${FIXTURES}/field_route.json" --noise realistic)
