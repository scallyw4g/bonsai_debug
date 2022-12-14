
struct render_state
{
  window_layout*  Window;
  u32 WindowStartCommandIndex;

  rect2 ClipRect;

  layout* Layout;

  b32 Hover;
  b32 Pressed;
  b32 Clicked;
};

struct debug_text_render_group
{
  u32 SolidUIVertexBuffer;
  u32 SolidUIColorBuffer;
  u32 SolidUIUVBuffer;

  texture *DebugTextureArray;
  shader Text2DShader;
  s32 TextTextureUniform;
  textured_2d_geometry_buffer Geo;
  shader DebugFontTextureShader;
  shader SolidUIShader;
};

struct input;
struct debug_ui_render_group
{
  debug_text_render_group *TextGroup;

  u64 InteractionStackTop;

  v2 *MouseP;
  v2 *MouseDP;
  v2 ScreenDim;
  input *Input;

  window_layout *HighestWindow; // NOTE(Jesse): Highest in terms of InteractionStackIndex

#define MAX_MINIMIZED_WINDOWS 64
  window_layout *MinimizedWindowBuffer[MAX_MINIMIZED_WINDOWS];

  umm HoverInteractionId;
  umm ClickedInteractionId;
  umm PressedInteractionId;

  u32 SolidGeoCountLastFrame;
  u32 TextGeoCountLastFrame;

  untextured_2d_geometry_buffer Geo;

  ui_render_command_buffer *CommandBuffer;

  memory_arena RenderCommandArena;

#define RANDOM_COLOR_COUNT 128
  v3 DebugColors[RANDOM_COLOR_COUNT];
};

