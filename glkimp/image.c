#include "glkimp.h"

extern char gli_workdir[];


static int loadimage(int image)
{
    glui32 chunktype;
    FILE *file;
    char *buf;
    long pos;
    long len;
    
    if (win_findimage(image))
        return TRUE;
    
    if (!giblorb_is_resource_map())
    {
        char filename[1024];

        sprintf(filename, "%s/PIC%d", gli_workdir, image);

        fprintf(stderr, "loadimage %s", filename);

        file = fopen(filename, "rb");
        if (!file)
            return FALSE;

        fseek(file, 0, 2);
        len = ftell(file);
        fseek(file, 0, 0);

        buf = malloc(len);
        if (!buf)
        {
            fclose(file);
            return FALSE;
        }
        
        fread(buf, len, 1, file);

        fclose(file);
    }
    else
    {
        giblorb_get_resource(giblorb_ID_Pict, image, &file, &pos, &len, &chunktype);
        if (!file)
        {
            gli_strict_warning("Failed to get resource from blorb!\n");
            return FALSE;
        }
        
        buf = malloc(len);
        if (!buf)
            return FALSE;
        
        fseek(file, pos, 0);
        fread(buf, len, 1, file);
    }
    
    win_loadimage(image, buf, (int)len);
    
    free(buf);
    
    return TRUE;
}


glui32 glk_image_draw_scaled(winid_t win, glui32 image,
			     glsi32 val1, glsi32 val2, glui32 width, glui32 height)
{
    if (!win)
    {
        gli_strict_warning("image_draw_scaled: invalid ref");
        return FALSE;
    }

    if (!loadimage(image))
    {
        return FALSE;
    }

    win_drawimage(win->peer, val1, val2, width, height);
    return TRUE;
}

glui32 glk_image_draw(winid_t win, glui32 image, glsi32 val1, glsi32 val2)
{
    if (!win)
    {
        gli_strict_warning("image_draw: invalid ref");
        return FALSE;
    }

    return glk_image_draw_scaled(win, image, val1, val2, 0, 0);
}

glui32 glk_image_get_info(glui32 image, glui32 *width, glui32 *height)
{
    if (width) *width = 1;
    if (height) *height = 1;
    
    if (!loadimage(image))
    {
        fprintf(stderr, "glk_image_get_info: loadimage(%d) FAILED\n", image);
        return FALSE;
    }

    win_sizeimage(width, height);

    return TRUE;
}

void glk_window_flow_break(winid_t win)
{
    if (!win)
    {
        gli_strict_warning("window_erase_rect: invalid ref");
        return;
    }
    if (win->type != wintype_TextBuffer)
    {
        gli_strict_warning("window_flow_break: not a text buffer window");
        return;
    }
    win_flowbreak(win->peer);
}

void glk_window_erase_rect(winid_t win,
			   glsi32 left, glsi32 top, glui32 width, glui32 height)
{
    if (!win)
    {
        gli_strict_warning("window_erase_rect: invalid ref");
        return;
    }
    if (win->type != wintype_Graphics)
    {
        gli_strict_warning("window_erase_rect: not a graphics window");
        return;
    }
    win_fillrect(win->peer, win->background, left, top, width, height);
}

void glk_window_fill_rect(winid_t win, glui32 color,
			  glsi32 left, glsi32 top, glui32 width, glui32 height)
{
    if (!win)
    {
        gli_strict_warning("window_fill_rect: invalid ref");
        return;
    }
    if (win->type != wintype_Graphics)
    {
        gli_strict_warning("window_fill_rect: not a graphics window");
        return;
    }
    
    win_fillrect(win->peer, color, left, top, width, height);
}

void glk_window_set_background_color(winid_t win, glui32 color)
{
    if (!win)
    {
        gli_strict_warning("window_set_background_color: invalid ref");
        return;
    }

    if (win->type != wintype_Graphics)
    {
        gli_strict_warning("window_set_background_color: not a graphics window");
        return;
    }

    win->background = color;
    win_setbgnd(win->peer, color);
}
