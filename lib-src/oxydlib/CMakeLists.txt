add_library(oxydlib
        Bitmap.cpp
        Bitmap.h
        DatFile.cpp
        DatFile.h
        FileUtils.cpp
        FileUtils.h
        Level.cpp
        Level.h
        OxydVersion.h
        VecUtils.cpp
        VecUtils.h
)

add_executable(extractbitmaps
        extractbitmaps.cpp
)
target_link_libraries(extractbitmaps oxydlib)

add_executable(extractchunks
        extractchunks.cpp
)
target_link_libraries(extractchunks oxydlib)

add_executable(printlevels
        printlevels.cpp
)
target_link_libraries(printlevels oxydlib)

add_executable(printstats
        printstats.cpp
        EnigmaNames.cpp
        EnigmaNames.h
)

target_link_libraries(printstats oxydlib)
