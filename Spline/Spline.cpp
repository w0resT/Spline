// Backend + GUI
#include "imgui/imgui.h"
#include "imgui/backend/imgui_impl_dx9.h"
#include "imgui/backend/imgui_impl_win32.h"
#include <d3d9.h>
#include <tchar.h>

#pragma comment(lib, "d3d9.lib")

// Other includes
#include <iostream>
#include <vector>

#include "types/vec2.h"

namespace globals {
    std::vector<vec2> g_points;

    bool is_splined = false;
}

// Data
static LPDIRECT3D9              g_pD3D = NULL;
static LPDIRECT3DDEVICE9        g_pd3dDevice = NULL;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

// Forward declarations of helper functions
bool CreateDeviceD3D( HWND hWnd );
void CleanupDeviceD3D( );
void ResetDevice( );
LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

int get_point_hovered( const std::vector<vec2> &points, vec2 mouse_pos, int radius ) {
    // Try to use ImGui::IsMouseHoveringRect()
    for ( size_t i = 0; i < points.size( ); ++i ) {
        // Checking for the same pos +- radius
        if ( std::fabsf( points[ i ].x - mouse_pos.x ) <= radius
             && std::fabsf( points[ i ].y - mouse_pos.y ) <= radius ) {
            return i;
        }
    }
    return -1;
}

static void ShowMainWindow( bool *p_open ) {
    const ImGuiViewport *viewport = ImGui::GetMainViewport( );
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 work_size = viewport->WorkSize;
    ImGui::SetNextWindowPos( work_pos );
    ImGui::SetNextWindowSize( work_size );
    if ( !ImGui::Begin( "Yurchuk | Interpolation splines", NULL, ImGuiWindowFlags_NoMove
         | ImGuiWindowFlags_NoResize
         | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings
         | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) ) {
        ImGui::End( );
        return;
    }

    if ( ImGui::BeginTable( "##main_page.table", 2, ImGuiTableFlags_NoSavedSettings ) ) {
        // First column
        ImGui::TableNextColumn( );
        {
            ImGui::Text( "Options" );
            if ( ImGui::BeginChild( "##main_page.child.left.up", ImVec2( 0, work_size.y / 2 - 38 ), true, ImGuiWindowFlags_NoSavedSettings ) ) {
                static float bt_sz_x = 160.f;
                static float bt_sz_y = 0;

                if ( ImGui::Button( "Clear canvas", ImVec2( bt_sz_x, bt_sz_y ) ) ) {
                    globals::g_points.clear( );
                    globals::is_splined = false;
                }

                if ( ImGui::Button( "Do spline", ImVec2( bt_sz_x, bt_sz_y ) ) ) {
                    globals::is_splined = true;
                }

                ImGui::EndChild( );
            }

            if ( ImGui::BeginChild( "##main_page.child.left.down", ImVec2( 0, 0 ), true, ImGuiWindowFlags_NoSavedSettings ) ) {
                const ImU32 main_line_color_u32 = ImColor( 255, 255, 102, 255 );
                const ImU32 new_line_color_u32 = ImColor( 255, 179, 102, 255 );

                // Using InvisibleButton() as a convenience 1) it will advance the layout cursor and 2) allows us to use IsItemHovered()/IsItemActive()
                ImVec2 canvas_p0 = ImGui::GetCursorScreenPos( );      // ImDrawList API uses screen coordinates!
                ImVec2 canvas_sz = ImGui::GetContentRegionAvail( );   // Resize canvas to what's available
                if ( canvas_sz.x < 50.0f ) canvas_sz.x = 50.0f;
                if ( canvas_sz.y < 50.0f ) canvas_sz.y = 50.0f;
                ImVec2 canvas_p1 = ImVec2( canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y );

                // Draw border and background color
                ImGuiIO &io = ImGui::GetIO( );
                ImDrawList *draw_list = ImGui::GetWindowDrawList( );
                draw_list->AddRectFilled( canvas_p0, canvas_p1, IM_COL32( 50, 50, 50, 255 ) );
                draw_list->AddRect( canvas_p0, canvas_p1, IM_COL32( 255, 255, 255, 255 ) );

                // This will catch our interactions
                ImGui::InvisibleButton( "canvas", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight );
                const bool is_hovered = ImGui::IsItemHovered( ); // Hovered
                const bool is_active = ImGui::IsItemActive( );   // Held
                const vec2 origin( canvas_p0.x, canvas_p0.y ); // Lock scrolled origin
                const vec2 mouse_pos_in_canvas( io.MousePos.x - origin.x, io.MousePos.y - origin.y );

                static bool moving_point = false;
                static int moving_point_idx = -1;

                // Add first and second point
                if ( is_hovered && moving_point_idx == -1 && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) ) {
                    // No new points if we already have a spline
                    if ( globals::is_splined ) {
                        // Moving points
                        auto idx = get_point_hovered( globals::g_points, mouse_pos_in_canvas, 3 );
                        if ( idx != -1 ) {
                            moving_point_idx = idx;
                        }
                    }
                    else {
                        // Push point
                        globals::g_points.push_back( mouse_pos_in_canvas );
                    }
                }

                // Updating pos for moving point
                if ( moving_point_idx != -1 ) {
                    globals::g_points[ moving_point_idx ] = mouse_pos_in_canvas;
                    if ( !ImGui::IsMouseDown( ImGuiMouseButton_Left ) )
                        moving_point_idx = -1;
                }

                // Draw grid + all lines in the canvas
                draw_list->PushClipRect( canvas_p0, canvas_p1, true );

                // Drawing grid
                const float GRID_STEP = 54.0f;
                for ( float x = GRID_STEP; x < canvas_sz.x; x += GRID_STEP )
                    draw_list->AddLine( ImVec2( canvas_p0.x + x, canvas_p0.y ), ImVec2( canvas_p0.x + x, canvas_p1.y ), IM_COL32( 200, 200, 200, 40 ) );
                for ( float y = GRID_STEP; y < canvas_sz.y; y += GRID_STEP )
                    draw_list->AddLine( ImVec2( canvas_p0.x, canvas_p0.y + y ), ImVec2( canvas_p1.x, canvas_p0.y + y ), IM_COL32( 200, 200, 200, 40 ) );

                // Drawing lines
                if ( globals::g_points.size( ) > 1 ) {
                    for ( size_t n = 0; n < globals::g_points.size( ) - 1; ++n )
                        draw_list->AddLine( ImVec2( origin.x + globals::g_points[ n ].x, origin.y + globals::g_points[ n ].y ),
                                            ImVec2( origin.x + globals::g_points[ n + 1 ].x, origin.y + globals::g_points[ n + 1 ].y ), main_line_color_u32, 2.0f );
                }

                // Drawing circle on dots
                for ( size_t n = 0; n < globals::g_points.size( ); ++n ) {
                    draw_list->AddCircle( ImVec2( origin.x + globals::g_points[ n ].x, origin.y + globals::g_points[ n ].y ), 3.f, IM_COL32( 59, 184, 42, 255 ), 0, 3.f );
                }

                draw_list->PopClipRect( );

                ImGui::EndChild( );
            }
        }

        // Second column
        ImGui::TableNextColumn( );
        {
            ImGui::Text( "Information" );
            if ( ImGui::BeginChild( "##main_page.child.right", ImVec2( work_size.x / 2 - 12, 0 ), true, ImGuiWindowFlags_NoSavedSettings ) ) {


                ImGui::EndChild( );
            }
        }

        ImGui::EndTable( );
    }

    ImGui::End( );
}

// Main code
int main( int, char ** ) {
    // Create application window
    WNDCLASSEX wc = { sizeof( WNDCLASSEX ), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle( NULL ), NULL, NULL, NULL, NULL, _T( "Yurchuk" ), NULL };
    ::RegisterClassEx( &wc );
    HWND hwnd = ::CreateWindow( wc.lpszClassName, _T( "Yurchuk | Interpolation splines" ), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL );

    // Initialize Direct3D
    if ( !CreateDeviceD3D( hwnd ) ) {
        CleanupDeviceD3D( );
        ::UnregisterClass( wc.lpszClassName, wc.hInstance );
        return 1;
    }

    // Show the window
    ::ShowWindow( hwnd, SW_SHOWDEFAULT );
    ::UpdateWindow( hwnd );

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION( );
    ImGui::CreateContext( );
    ImGuiIO &io = ImGui::GetIO( ); ( void )io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark( );
    //ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle &style = ImGui::GetStyle( );
    if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
        style.WindowRounding = 0.0f;
        style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init( hwnd );
    ImGui_ImplDX9_Init( g_pd3dDevice );

    // Our state
    bool show_app_main_window = true;
    ImVec4 clear_color = ImVec4( 0.45f, 0.55f, 0.60f, 1.00f );

    // Main loop
    bool done = false;
    while ( !done ) {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while ( ::PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE ) ) {
            ::TranslateMessage( &msg );
            ::DispatchMessage( &msg );
            if ( msg.message == WM_QUIT )
                done = true;
        }
        if ( done )
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame( );
        ImGui_ImplWin32_NewFrame( );
        ImGui::NewFrame( );

        if ( show_app_main_window ) {
            ShowMainWindow( &show_app_main_window );
        }

        // Rendering
        ImGui::EndFrame( );
        g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
        g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
        g_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE );
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA( ( int )( clear_color.x * clear_color.w * 255.0f ), ( int )( clear_color.y * clear_color.w * 255.0f ), ( int )( clear_color.z * clear_color.w * 255.0f ), ( int )( clear_color.w * 255.0f ) );
        g_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0 );
        if ( g_pd3dDevice->BeginScene( ) >= 0 ) {
            ImGui::Render( );
            ImGui_ImplDX9_RenderDrawData( ImGui::GetDrawData( ) );
            g_pd3dDevice->EndScene( );
        }

        // Update and Render additional Platform Windows
        if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
            ImGui::UpdatePlatformWindows( );
            ImGui::RenderPlatformWindowsDefault( );
        }

        HRESULT result = g_pd3dDevice->Present( NULL, NULL, NULL, NULL );

        // Handle loss of D3D9 device
        if ( result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel( ) == D3DERR_DEVICENOTRESET )
            ResetDevice( );
    }

    ImGui_ImplDX9_Shutdown( );
    ImGui_ImplWin32_Shutdown( );
    ImGui::DestroyContext( );

    CleanupDeviceD3D( );
    ::DestroyWindow( hwnd );
    ::UnregisterClass( wc.lpszClassName, wc.hInstance );

    return 0;
}

// Helper functions

bool CreateDeviceD3D( HWND hWnd ) {
    if ( ( g_pD3D = Direct3DCreate9( D3D_SDK_VERSION ) ) == NULL )
        return false;

    // Create the D3DDevice
    ZeroMemory( &g_d3dpp, sizeof( g_d3dpp ) );
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
    if ( g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice ) < 0 )
        return false;

    return true;
}

void CleanupDeviceD3D( ) {
    if ( g_pd3dDevice ) { g_pd3dDevice->Release( ); g_pd3dDevice = NULL; }
    if ( g_pD3D ) { g_pD3D->Release( ); g_pD3D = NULL; }
}

void ResetDevice( ) {
    ImGui_ImplDX9_InvalidateDeviceObjects( );
    HRESULT hr = g_pd3dDevice->Reset( &g_d3dpp );
    if ( hr == D3DERR_INVALIDCALL )
        IM_ASSERT( 0 );
    ImGui_ImplDX9_CreateDeviceObjects( );
}

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
    if ( ImGui_ImplWin32_WndProcHandler( hWnd, msg, wParam, lParam ) )
        return true;

    switch ( msg ) {
    case WM_SIZE:
        if ( g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED ) {
            g_d3dpp.BackBufferWidth = LOWORD( lParam );
            g_d3dpp.BackBufferHeight = HIWORD( lParam );
            ResetDevice( );
        }
        return 0;
    case WM_SYSCOMMAND:
        if ( ( wParam & 0xfff0 ) == SC_KEYMENU ) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage( 0 );
        return 0;
    case WM_DPICHANGED:
        if ( ImGui::GetIO( ).ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports ) {
            //const int dpi = HIWORD(wParam);
            //printf("WM_DPICHANGED to %d (%.0f%%)\n", dpi, (float)dpi / 96.0f * 100.0f);
            const RECT *suggested_rect = ( RECT * )lParam;
            ::SetWindowPos( hWnd, NULL, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE );
        }
        break;
    }
    return ::DefWindowProc( hWnd, msg, wParam, lParam );
}
