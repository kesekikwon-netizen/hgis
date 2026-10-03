#pragma once
// The commit the build was configured from, or "unknown" (F138). It lives in one generated source
// (build/generated/KaGitHash.cpp from cmake/KaGitHash.cpp.in), so a new commit recompiles that file
// alone; as a compile definition on ka_core it recompiled every file of the app and the tests.
const char* kaHgisGitHash();
