# NOTE(Jesse): This is not designed to be a standalone build script, but to be
# included in builds that already utilize the bonsai build system

function BuildDebugSystem
{
  echo ""
  ColorizeTitle "DebugSystem"
  DEBUG_SRC_FILE="$INCLUDE/bonsai_debug/debug.cpp"
  output_basename="$BIN/lib_debug_system"
  echo -e "$Building $DEBUG_SRC_FILE"
  clang++                    \
    $OPTIMIZATION_LEVEL      \
    $CXX_OPTIONS             \
    $BONSAI_INTERNAL         \
    $PLATFORM_CXX_OPTIONS    \
    $PLATFORM_LINKER_OPTIONS \
    $PLATFORM_DEFINES        \
    $PLATFORM_INCLUDE_DIRS   \
    $SHARED_LIBRARY_FLAGS    \
    -I "$ROOT"               \
    -I "$SRC"                \
    -I "$INCLUDE"            \
    -o $output_basename      \
    "$DEBUG_SRC_FILE" &&     \
    mv "$output_basename" "$output_basename""_loadable""$PLATFORM_LIB_EXTENSION" &
  TrackPid "$output_basename" $!
}
