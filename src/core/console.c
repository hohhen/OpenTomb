
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL_platform.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_keycode.h>

#include "gl_font.h"
#include "gl_util.h"
#include "console.h"
#include "system.h"
#include "vmath.h"

static console_info_t           con_base;
static gl_font_manager_p        con_font_manager = NULL;

static void Con_DrawBackground();
static void Con_DrawCursor();

int  Engine_ExecCmd(char *ch);

void Con_Init()
{
    uint16_t i;
    
    con_base.log_pos = 0;

    // lines count check
    if(con_base.line_count < CON_MIN_LOG)
    {
        con_base.line_count = CON_MIN_LOG;
    }
    if(con_base.line_count > CON_MAX_LOG)
    {
        con_base.line_count = CON_MAX_LOG;
    }

    // log size check
    if(con_base.log_lines_count < CON_MIN_LOG)
    {
        con_base.log_lines_count = CON_MIN_LOG;
    }
    if(con_base.log_lines_count > CON_MAX_LOG)
    {
        con_base.log_lines_count = CON_MAX_LOG;
    }

    // showing lines count check
    if(con_base.showing_lines < CON_MIN_LINES)
    {
        con_base.showing_lines = CON_MIN_LINES;
    }
    if(con_base.showing_lines > CON_MAX_LINES)
    {
        con_base.showing_lines = CON_MAX_LINES;
    }

    // spacing check
    if(con_base.spacing < CON_MIN_LINE_INTERVAL)
    {
        con_base.spacing = CON_MIN_LINE_INTERVAL;
    }
    if(con_base.spacing > CON_MAX_LINE_INTERVAL)
    {
        con_base.spacing = CON_MAX_LINE_INTERVAL;
    }

    // linesize check
    if(con_base.line_size < CON_MIN_LINE_SIZE)
    {
        con_base.line_size = CON_MIN_LINE_SIZE;
    }
    if(con_base.line_size > CON_MAX_LINE_SIZE)
    {
        con_base.line_size = CON_MAX_LINE_SIZE;
    }

    con_base.line_text  = (char**)malloc(con_base.line_count*sizeof(char*));
    con_base.line_style_id = (uint16_t*)malloc(con_base.line_count * sizeof(uint16_t));

    for(i=0;i<con_base.line_count;i++)
    {
        con_base.line_text[i]  = (char*) calloc(con_base.line_size*sizeof(char), 1);
        con_base.line_style_id[i] = FONTSTYLE_GENERIC;
    }

    con_base.log_lines = (char**) realloc(con_base.log_lines, con_base.log_lines_count * sizeof(char*));
    for(i=0;i<con_base.log_lines_count;i++)
    {
        con_base.log_lines[i] = (char*) calloc(con_base.line_size * sizeof(char), 1);
    }

    con_font_manager = glf_create_manager(GUI_MAX_FONTS, GUI_MAX_FONTSTYLES);
}


void Con_InitFont()
{
    if(con_font_manager->fonts[FONT_CONSOLE].gl_font == NULL)
    {
        glf_manager_add_font(con_font_manager, 0, 12, "resource/fonts/DroidSansMono.ttf");
    }
    if(con_font_manager->fonts[FONT_CONSOLE].gl_font)
    {
        Con_SetLineInterval(con_base.spacing);
    }
}


void Con_InitGlobals()
{
    con_base.background_color[0] = 1.0;
    con_base.background_color[1] = 0.9;
    con_base.background_color[2] = 0.7;
    con_base.background_color[3] = 0.4;

    con_base.log_lines_count = CON_MIN_LOG;
    con_base.log_lines       = NULL;

    con_base.spacing         = CON_MIN_LINE_INTERVAL;
    con_base.line_count      = CON_MIN_LINES;
    con_base.line_size       = CON_MIN_LINE_SIZE;
    con_base.line_text       = NULL;
    con_base.line_style_id   = NULL;

    con_base.showing_lines = con_base.line_count;
    con_base.show_cursor_period = 0.5;
    con_base.cursor_pos = 0;
}


void Con_Destroy()
{
    uint16_t i;

    for(i=0;i<con_base.line_count;i++)
    {
        free(con_base.line_text[i]);
    }
    free(con_base.line_text);
    free(con_base.line_style_id);

    for(i=0;i<con_base.log_lines_count;i++)
    {
        free(con_base.log_lines[i]);
    }
    free(con_base.log_lines);
    con_base.log_lines = NULL;

    if(con_font_manager)
    {
        glf_free_manager(con_font_manager);
        con_font_manager = NULL;
    }
}


console_info_p Con_GetConsole()
{
    return &con_base;
}


float Con_GetLineInterval()
{
    return con_base.spacing;
}


void Con_SetLineInterval(float interval)
{
    if(!con_font_manager || (con_font_manager->fonts[FONT_CONSOLE].gl_font == NULL) ||
       (interval < CON_MIN_LINE_INTERVAL) || (interval > CON_MAX_LINE_INTERVAL))
    {
        return; // nothing to do
    }

    con_base.spacing = interval;
    // con_base.font->font_size has absolute size (after scaling)
    con_base.line_height = (1.0 + con_base.spacing) * con_font_manager->fonts[FONT_CONSOLE].gl_font->font_size;
    con_base.cursor_x = 8 + 1;
    con_base.cursor_y = screen_info.h - con_base.line_height * con_base.showing_lines;
    if(con_base.cursor_y < 8)
    {
        con_base.cursor_y = 8;
    }
}


uint16_t Con_GetShowingLines()
{
    return con_base.showing_lines;
}


void Con_SetShowingLines(uint16_t value)
{
    if((value >=2 ) && (value <= con_base.line_count))
    {
        con_base.showing_lines = value;
        con_base.cursor_y = screen_info.h - con_base.line_height * con_base.showing_lines;
    }
}


void Con_Filter(char *text)
{
    if(text != NULL)
    {
        while(*text != '\0')
        {
            Con_Edit(*text);
            text++;
        }
    }
}


void Con_Edit(int key)
{
    if(key == SDLK_UNKNOWN || key == SDLK_BACKQUOTE || key == SDLK_BACKSLASH)
    {
        return;
    }

    if((key == SDLK_RETURN) && con_base.line_text[0])
    {
        Con_AddLog(con_base.line_text[0]);
        if(!Engine_ExecCmd(con_base.line_text[0]))
        {
            //Con_AddLine(con_base.text[0]);
        }
        con_base.line_text[0][0] = 0;
        con_base.cursor_pos = 0;
        con_base.cursor_x = 8 + 1;
        return;
    }

    con_base.cursor_time = 0.0;
    con_base.show_cursor = 1;

    int16_t oldLength = utf8_strlen(con_base.line_text[0]);    // int16_t is absolutly enough

    switch(key)
    {
        case SDLK_UP:
            //Audio_Send(lua_GetGlobalSound(engine_lua, TR_AUDIO_SOUND_GLOBALID_MENUPAGE));
            if(*con_base.log_lines[con_base.log_pos])
            {
                strncpy(con_base.line_text[0], con_base.log_lines[con_base.log_pos], con_base.line_size);
                con_base.log_pos++;
                if((!*con_base.log_lines[con_base.log_pos]) || (con_base.log_pos >= con_base.log_lines_count))
                {
                    con_base.log_pos = 0;
                }
                con_base.cursor_pos = utf8_strlen(con_base.line_text[0]);
            }
            break;

        case SDLK_DOWN:
            //Audio_Send(lua_GetGlobalSound(engine_lua, TR_AUDIO_SOUND_GLOBALID_MENUPAGE));
            if(con_base.log_pos == 0)
            {
                for(;(con_base.log_pos < con_base.log_lines_count) && *con_base.log_lines[con_base.log_pos];con_base.log_pos++);
            }
            if((con_base.log_pos > 0) && *con_base.log_lines[con_base.log_pos-1])
            {
                strncpy(con_base.line_text[0], con_base.log_lines[con_base.log_pos-1], con_base.line_size);
                con_base.log_pos--;
                con_base.cursor_pos = utf8_strlen(con_base.line_text[0]);
            }
            break;

        case SDLK_LEFT:
            if(con_base.cursor_pos > 0)
            {
                con_base.cursor_pos--;
            }
            break;

        case SDLK_RIGHT:
            if(con_base.cursor_pos < oldLength)
            {
                con_base.cursor_pos++;
            }
            break;

        case SDLK_HOME:
            con_base.cursor_pos = 0;
            break;

        case SDLK_END:
            con_base.cursor_pos = oldLength;
            break;

        case SDLK_BACKSPACE:
            if(con_base.cursor_pos > 0)
            {
                int16_t i;
                for(i = con_base.cursor_pos; i < oldLength ;i++)
                {
                    con_base.line_text[0][i-1] = con_base.line_text[0][i];
                }
                con_base.line_text[0][oldLength-1] = 0;
                con_base.line_text[0][oldLength] = 0;
                con_base.cursor_pos--;
            }
            break;

        case SDLK_DELETE:
            if(/*(con_base.cursor_pos > 0) && */(con_base.cursor_pos < oldLength))
            {
                int16_t i;
                for(i=con_base.cursor_pos;i<oldLength-1;i++)
                {
                    con_base.line_text[0][i] = con_base.line_text[0][i+1];
                }
                con_base.line_text[0][oldLength-1] = 0;
            }
            break;

        default:
            if((oldLength < con_base.line_size-1) && (key >= SDLK_SPACE))
            {
                int16_t i;
                for(i=oldLength;i>con_base.cursor_pos;i--)
                {
                    con_base.line_text[0][i] = con_base.line_text[0][i-1];
                }
                con_base.line_text[0][con_base.cursor_pos] = key;
                con_base.line_text[0][++oldLength] = 0;
                con_base.cursor_pos++;
            }
            break;
    }

    Con_CalcCursorPosition();
}


void Con_CalcCursorPosition()
{
    if(con_font_manager && (con_font_manager->fonts[FONT_CONSOLE].gl_font))
    {
        con_base.cursor_x = 8 + 1 + glf_get_string_len(con_font_manager->fonts[FONT_CONSOLE].gl_font, con_base.line_text[0], con_base.cursor_pos);
    }
}


void Con_AddLog(const char *text)
{
    if((text != NULL) && (text[0] != 0))
    {
        if(con_base.log_lines_count > 1)
        {
            char *last = con_base.log_lines[con_base.log_lines_count-1];        // save pointer to the last log string
            uint16_t i;
            for(i=con_base.log_lines_count-1;i>0;i--)                           // shift log
            {
                con_base.log_lines[i] = con_base.log_lines[i-1];                // shift is round
            }
            con_base.log_lines[0] = last;                                       // cycle the shift
            strncpy(con_base.log_lines[0], text, con_base.line_size);
            con_base.log_lines[0][con_base.line_size-1] = 0;                    // paranoid end of string
        }
        else
        {
            strncpy(con_base.log_lines[0], text, con_base.line_size);
            con_base.log_lines[0][con_base.line_size-1] = 0;                    // paranoid end of string
        }

        con_base.log_pos = 0;
    }
}


void Con_AddLine(const char *text, uint16_t font_style)
{
    if(text != NULL)
    {
        size_t len = 0;
        do
        {
            char *last = con_base.line_text[con_base.line_count-1];             // save pointer to the last log string
            uint16_t i;

            len = strlen(text);
            for(i=con_base.line_count-1;i>1;i--)                                // shift log
            {
                con_base.line_style_id[i] = con_base.line_style_id[i-1];
                con_base.line_text[i]  = con_base.line_text[i-1];               // shift is round
            }

            con_base.line_text[1] = last;                                       // cycle the shift
            con_base.line_style_id[1] = font_style;
            strncpy(con_base.line_text[1], text, con_base.line_size);
            con_base.line_text[1][con_base.line_size-1] = 0;                    // paranoid end of string
            text += con_base.line_size-1;
        }
        while(len >= con_base.line_size);
    }
}


void Con_AddText(const char *text, uint16_t font_style)
{
    if(text != NULL)
    {
        char buf[4096], ch;
        size_t i, j, text_size = strlen(text);

        buf[0] = 0;
        for(i=0,j=0;i<text_size;i++)
        {
            ch = text[i];
            if((ch == 10) || (ch == 13))
            {
                j = (j < 4096)?(j):(4095);
                buf[j] = 0;
                buf[4095] = 0;
                if((j > 0) && ((buf[0] != 10) && (buf[0] != 13) && ((buf[0] > 31) || (buf[1] > 32))))
                {
                    Con_AddLine(buf, font_style);
                }
                j=0;
            }
            else if(j < 4096)
            {
               buf[j++] = ch;
            }
        }

        buf[4095] = 0;
        if(j < 4096)
        {
            buf[j] = 0;
        }
        if((j > 0) && ((buf[0] != 10) && (buf[0] != 13) && ((buf[0] > 31) || (buf[1] > 32))))
        {
            Con_AddLine(buf, font_style);
        }
    }
}


void Con_Printf(const char *fmt, ...)
{
    va_list argptr;
    char buf[4096];

    va_start(argptr, fmt);
    vsnprintf(buf, 4096, fmt, argptr);
    buf[4096-1] = 0;
    va_end(argptr);
    Con_AddLine(buf, FONTSTYLE_CONSOLE_NOTIFY);
}


void Con_Warning(const char *fmt, ...)
{
    va_list argptr;
    char buf[4096];

    va_start(argptr, fmt);
    vsnprintf(buf, 4096, fmt, argptr);
    buf[4096-1] = 0;
    va_end(argptr);
    Con_AddLine(buf, FONTSTYLE_CONSOLE_WARNING);
}


void Con_Notify(const char *fmt, ...)
{
    va_list argptr;
    char buf[4096];

    va_start(argptr, fmt);
    vsnprintf(buf, 4096, fmt, argptr);
    buf[4096-1] = 0;
    va_end(argptr);
    Con_AddLine(buf, FONTSTYLE_CONSOLE_NOTIFY);
}


int Con_AddFont(uint16_t index, uint16_t size, const char* path)
{
    if(con_font_manager)
    {
        return glf_manager_add_font(con_font_manager, index, size, path);
    }
    return 0;
}


int Con_RemoveFont(uint16_t index)
{
    if(con_font_manager)
    {
        return glf_manager_remove_font(con_font_manager, index);
    }
    return 0;
}


int Con_AddFontStyle(uint16_t index,
                     GLfloat R, GLfloat G, GLfloat B, GLfloat A,
                     uint8_t shadow, uint8_t rect, uint8_t rect_border,
                     GLfloat rect_R, GLfloat rect_G, GLfloat rect_B, GLfloat rect_A)
{
    if(con_font_manager)
    {
        return glf_manager_add_fontstyle(con_font_manager, index, R, G, B, A,
                                         shadow, rect, rect_border,
                                         rect_R, rect_G, rect_B, rect_A);
    }
    return 0;
}


int Con_RemoveFontStyle(uint16_t index)
{
    if(con_font_manager)
    {
        return glf_manager_remove_fontstyle(con_font_manager, index);
    }
    return 0;
}


gl_tex_font_p  Con_GetFont(uint16_t index)
{
    if(con_font_manager)
    {
        return glf_manager_get_font(con_font_manager, index);
    }
    return NULL;
}


gl_fontstyle_p Con_GetFontStyle(uint16_t index)
{
    if(con_font_manager)
    {
        return glf_manager_get_style(con_font_manager, index);
    }
    return NULL;
}


void Con_SetScaleFonts(float scale)
{
    if(con_font_manager)
    {
        glf_manager_resize(con_font_manager, scale);
        Con_SetLineInterval(con_base.spacing);
    }
}


void Con_Clean()
{
    for(uint16_t i=0;i<con_base.line_count;i++)
    {
        con_base.line_text[i][0]  = 0;
        con_base.line_style_id[i] = FONTSTYLE_GENERIC;
    }
}

/*
 * CONSOLE DRAW FUNCTIONS
 */
void Con_Draw(float time)
{
    gl_tex_font_p glf = Con_GetFont(0);
    if(glf && con_base.show_console)
    {
        int x = 8;
        int y = con_base.cursor_y;
        
        con_base.cursor_time += time;
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        BindWhiteTexture();
        Con_DrawBackground();
        Con_DrawCursor();
        
        for(uint16_t i=0;i<con_base.showing_lines;i++)
        {
            GLfloat *col = Con_GetFontStyle(con_base.line_style_id[i])->font_color;
            y += con_base.line_height;
            vec4_copy(glf->gl_font_color, col);
            glf_render_str(glf, (GLfloat)x, (GLfloat)y, con_base.line_text[i]);
        }
    }
}

void Con_DrawBackground()
{
    /*
     * Draw console background to see the text
     */
    GLfloat x0 = 0.0;
    GLfloat y0 = con_base.cursor_y + con_base.line_height - 8;
    GLfloat x1 = screen_info.w;
    GLfloat y1 = screen_info.h;
    GLfloat *v, backgroundArray[32];
    
    v = backgroundArray;
   *v++ = x0; *v++ = y0;
    vec4_copy(v, con_base.background_color);
    v += 4;
   *v++ = 0.0; *v++ = 0.0;
   
   *v++ = x1; *v++ = y0;
    vec4_copy(v, con_base.background_color);
    v += 4;
   *v++ = 0.0; *v++ = 0.0;
   
   *v++ = x1; *v++ = y1;
    vec4_copy(v, con_base.background_color);
    v += 4;
   *v++ = 0.0; *v++ = 0.0;
   
   *v++ = x0; *v++ = y1;
    vec4_copy(v, con_base.background_color);
    v += 4;
   *v++ = 0.0; *v++ = 0.0;
    
    glVertexPointer(2, GL_FLOAT, 8 * sizeof(GLfloat), backgroundArray);
    glColorPointer(4, GL_FLOAT, 8 * sizeof(GLfloat), backgroundArray + 2);
    glTexCoordPointer(2, GL_FLOAT, 8 * sizeof(GLfloat), backgroundArray + 6);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void Con_DrawCursor()
{
    GLint y = con_base.cursor_y + con_base.line_height;

    if(con_base.show_cursor_period)
    {
        if(con_base.cursor_time > con_base.show_cursor_period)
        {
            con_base.cursor_time = 0.0;
            con_base.show_cursor = !con_base.show_cursor;
        }
    }

    if(con_base.show_cursor)
    {
        GLfloat *v, cursor_array[16];
        v = cursor_array;
        
       *v++ = (GLfloat)con_base.cursor_x;                      
       *v++ = (GLfloat)y - 0.1 * (GLfloat)con_base.line_height;
        v[0] = 1.0; v[1] = 1.0; v[2] = 1.0; v[3] = 0.7;             v += 4;
        v[0] = 0.0; v[1] = 0.0;                                     v += 2;
       *v++ = (GLfloat)con_base.cursor_x;
       *v++ = (GLfloat)y + 0.7 * (GLfloat)con_base.line_height;
        v[0] = 1.0; v[1]= 1.0; v[2] = 1.0; v[3] = 0.7;              v += 4;
        v[0] = 0.0; v[1] = 0.0;
        glVertexPointer(2, GL_FLOAT, 8 * sizeof(GLfloat), cursor_array);
        glColorPointer(4, GL_FLOAT, 8 * sizeof(GLfloat), cursor_array + 2);
        glTexCoordPointer(2, GL_FLOAT, 8 * sizeof(GLfloat), cursor_array + 6);
        glDrawArrays(GL_LINES, 0, 2);
    }
}

int  Con_IsShown()
{
    return con_base.show_console;
}

void Con_SetShown(int value)
{
    con_base.show_console = value;
}