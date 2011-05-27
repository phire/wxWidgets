/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/minifram.cpp
// Purpose:
// Author:      Robert Roebling
// Id:          $Id: minifram.cpp 64404 2010-05-26 17:37:55Z RR $
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_MINIFRAME

#include "wx/minifram.h"

#ifndef WX_PRECOMP
    #include "wx/settings.h"
    #include "wx/dcclient.h"
    #include "wx/image.h"
#endif

#include "wx/gtk/dcclient.h"

#include <gtk/gtk.h>

//-----------------------------------------------------------------------------
// data
//-----------------------------------------------------------------------------

extern bool        g_blockEventsOnDrag;
extern bool        g_blockEventsOnScroll;

//-----------------------------------------------------------------------------
// "expose_event" of m_mainWidget
//-----------------------------------------------------------------------------

// StepColour() it a utility function that simply darkens
// or lightens a color, based on the specified percentage
static wxColor StepColour(const wxColor& c, int percent)
{
    int r = c.Red(), g = c.Green(), b = c.Blue();
    return wxColour((unsigned char)wxMin((r*percent)/100,255),
                    (unsigned char)wxMin((g*percent)/100,255),
                    (unsigned char)wxMin((b*percent)/100,255));
}

static wxColor LightContrastColour(const wxColour& c)
{
    int amount = 120;

    // if the color is especially dark, then
    // make the contrast even lighter
    if (c.Red() < 128 && c.Green() < 128 && c.Blue() < 128)
        amount = 160;

    return StepColour(c, amount);
}

extern "C" {
static gboolean gtk_window_own_expose_callback(GtkWidget* widget, GdkEventExpose* gdk_event, wxMiniFrame* win)
{
    if (!win->m_hasVMT || gdk_event->count > 0 ||
        gdk_event->window != gtk_widget_get_window(widget))
    {
        return false;
    }

    gtk_paint_shadow (gtk_widget_get_style(widget),
                      gdk_cairo_create(gtk_widget_get_window(widget)),
                      GTK_STATE_NORMAL,
                      GTK_SHADOW_OUT,
                      NULL, NULL, // FIXME: No clipping?
                      0, 0,
                      win->m_width, win->m_height);

    int style = win->GetWindowStyle();

    wxClientDC dc(win);

    wxDCImpl *impl = dc.GetImpl();
    wxClientDCImpl *gtk_impl = wxDynamicCast( impl, wxClientDCImpl );
    // gtk_impl->m_gdkwindow = widget->window; // Hack alert

    if (style & wxRESIZE_BORDER)
    {
        dc.SetBrush( *wxGREY_BRUSH );
        dc.SetPen( *wxTRANSPARENT_PEN );
        dc.DrawRectangle( win->m_width - 14, win->m_height-14, 14, 14 );
    }

    if (win->m_miniTitle && !win->GetTitle().empty())
    {
        dc.SetFont( *wxSMALL_FONT );

        wxBrush brush( LightContrastColour( wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT) ) );
        dc.SetBrush( brush );
        dc.SetPen( *wxTRANSPARENT_PEN );
        dc.DrawRectangle( win->m_miniEdge-1,
                          win->m_miniEdge-1,
                          win->m_width - (2*(win->m_miniEdge-1)),
                          15  );

        dc.SetTextForeground( *wxWHITE );
        dc.DrawText( win->GetTitle(), 6, 4 );

        if (style & wxCLOSE_BOX)
            dc.DrawBitmap( win->m_closeButton, win->m_width-18, 3, true );
    }

    return false;
}
}

//-----------------------------------------------------------------------------
// "button_press_event" of m_mainWidget
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
gtk_window_button_press_callback(GtkWidget* widget, GdkEventButton* gdk_event, wxMiniFrame* win)
{
    if (!win->m_hasVMT || gdk_event->window != gtk_widget_get_window(widget))
        return false;
    if (g_blockEventsOnDrag) return TRUE;
    if (g_blockEventsOnScroll) return TRUE;

    if (win->m_isDragging) return TRUE;

    int style = win->GetWindowStyle();

    int y = (int)gdk_event->y;
    int x = (int)gdk_event->x;

    if ((style & wxRESIZE_BORDER) &&
        (x > win->m_width-14) && (y > win->m_height-14))
    {
        GtkWidget *ancestor = gtk_widget_get_toplevel( widget );

        GdkWindow *source = gtk_widget_get_window(widget);

        int org_x = 0;
        int org_y = 0;
        gdk_window_get_origin( source, &org_x, &org_y );

        gtk_window_begin_resize_drag (GTK_WINDOW (ancestor),
                                  GDK_WINDOW_EDGE_SOUTH_EAST,
                                  1,
                                  org_x + x,
                                  org_y + y,
                                  0);

        return TRUE;
    }

    if (win->m_miniTitle && (style & wxCLOSE_BOX))
    {
        if ((y > 3) && (y < 19) && (x > win->m_width-19) && (x < win->m_width-3))
        {
            win->Close();
            return TRUE;
        }
    }

    if (y >= win->m_miniEdge + win->m_miniTitle)
        return true;

    gdk_window_raise( gtk_widget_get_window(win->m_widget) );

    gdk_pointer_grab( gtk_widget_get_window(widget), FALSE,
                      (GdkEventMask)
                         (GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK |
                          GDK_POINTER_MOTION_MASK        |
                          GDK_POINTER_MOTION_HINT_MASK  |
                          GDK_BUTTON_MOTION_MASK        |
                          GDK_BUTTON1_MOTION_MASK),
                      NULL,
                      NULL,
                      (unsigned int) GDK_CURRENT_TIME );

    win->m_diffX = x;
    win->m_diffY = y;
    win->m_oldX = 0;
    win->m_oldY = 0;

    win->m_isDragging = true;

    return TRUE;
}
}

//-----------------------------------------------------------------------------
// "button_release_event" of m_mainWidget
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
gtk_window_button_release_callback(GtkWidget* widget, GdkEventButton* gdk_event, wxMiniFrame* win)
{
    if (!win->m_hasVMT || gdk_event->window != gtk_widget_get_window(widget))
        return false;
    if (g_blockEventsOnDrag) return TRUE;
    if (g_blockEventsOnScroll) return TRUE;
    if (!win->m_isDragging) return TRUE;

    win->m_isDragging = false;

    int x = (int)gdk_event->x;
    int y = (int)gdk_event->y;

    gdk_pointer_ungrab ( (guint32)GDK_CURRENT_TIME );
    int org_x = 0;
    int org_y = 0;
    gdk_window_get_origin( gtk_widget_get_window(widget), &org_x, &org_y );
    x += org_x - win->m_diffX;
    y += org_y - win->m_diffY;
    win->m_x = x;
    win->m_y = y;
    gtk_window_move( GTK_WINDOW(win->m_widget), x, y );

    return TRUE;
}
}

//-----------------------------------------------------------------------------
// "leave_notify_event" of m_mainWidget
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
gtk_window_leave_callback(GtkWidget *widget,
                          GdkEventCrossing* gdk_event,
                          wxMiniFrame *win)
{
    if (!win->m_hasVMT) return FALSE;
    if (g_blockEventsOnDrag) return FALSE;
    if (gdk_event->window != gtk_widget_get_window(widget))
        return false;

    gdk_window_set_cursor( gtk_widget_get_window(widget), NULL );

    return FALSE;
}
}

//-----------------------------------------------------------------------------
// "motion_notify_event" of m_mainWidget
//-----------------------------------------------------------------------------

extern "C" {
static gboolean
gtk_window_motion_notify_callback( GtkWidget *widget, GdkEventMotion *gdk_event, wxMiniFrame *win )
{
    if (!win->m_hasVMT || gdk_event->window != gtk_widget_get_window(widget))
        return false;
    if (g_blockEventsOnDrag) return TRUE;
    if (g_blockEventsOnScroll) return TRUE;

    if (gdk_event->is_hint)
    {
       int x = 0;
       int y = 0;
       GdkModifierType state;
       gdk_window_get_pointer(gdk_event->window, &x, &y, &state);
       gdk_event->x = x;
       gdk_event->y = y;
       gdk_event->state = state;
    }

    int style = win->GetWindowStyle();

    int x = (int)gdk_event->x;
    int y = (int)gdk_event->y;

    if (!win->m_isDragging)
    {
        if (style & wxRESIZE_BORDER)
        {
            if ((x > win->m_width-14) && (y > win->m_height-14))
               gdk_window_set_cursor( gtk_widget_get_window(widget), gdk_cursor_new( GDK_BOTTOM_RIGHT_CORNER ) );
            else
               gdk_window_set_cursor( gtk_widget_get_window(widget), NULL );
            win->GTKUpdateCursor(false);
        }
        return TRUE;
    }

    win->m_oldX = x - win->m_diffX;
    win->m_oldY = y - win->m_diffY;

    int org_x = 0;
    int org_y = 0;
    gdk_window_get_origin( gtk_widget_get_window(widget), &org_x, &org_y );
    x += org_x - win->m_diffX;
    y += org_y - win->m_diffY;
    win->m_x = x;
    win->m_y = y;
    gtk_window_move( GTK_WINDOW(win->m_widget), x, y );

    return TRUE;
}
}

//-----------------------------------------------------------------------------
// wxMiniFrame
//-----------------------------------------------------------------------------

static unsigned char close_bits[]={
    0xff, 0xff, 0xff, 0xff, 0x07, 0xf0, 0xfb, 0xef, 0xdb, 0xed, 0x8b, 0xe8,
    0x1b, 0xec, 0x3b, 0xee, 0x1b, 0xec, 0x8b, 0xe8, 0xdb, 0xed, 0xfb, 0xef,
    0x07, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };


IMPLEMENT_DYNAMIC_CLASS(wxMiniFrame,wxFrame)

bool wxMiniFrame::Create( wxWindow *parent, wxWindowID id, const wxString &title,
      const wxPoint &pos, const wxSize &size,
      long style, const wxString &name )
{
    m_miniTitle = 0;
    if (style & wxCAPTION)
        m_miniTitle = 16;

    if (style & wxRESIZE_BORDER)
        m_miniEdge = 4;
    else
        m_miniEdge = 3;
    m_isDragging = false;
    m_oldX = -1;
    m_oldY = -1;
    m_diffX = 0;
    m_diffY = 0;

    wxFrame::Create( parent, id, title, pos, size, style, name );

    // Use a GtkEventBox for the title and borders. Using m_widget for this
    // almost works, except that setting the resize cursor has no effect.
    GtkWidget* eventbox = gtk_event_box_new();
    gtk_widget_add_events(eventbox,
        GDK_POINTER_MOTION_MASK |
        GDK_POINTER_MOTION_HINT_MASK);
    gtk_widget_show(eventbox);
    // Use a GtkAlignment to position m_mainWidget inside the decorations
    GtkWidget* alignment = gtk_alignment_new(0, 0, 1, 1);
    gtk_alignment_set_padding(GTK_ALIGNMENT(alignment),
        m_miniTitle + m_miniEdge, m_miniEdge, m_miniEdge, m_miniEdge);
    gtk_widget_show(alignment);
    // The GtkEventBox and GtkAlignment go between m_widget and m_mainWidget
    gtk_widget_reparent(m_mainWidget, alignment);
    gtk_container_add(GTK_CONTAINER(eventbox), alignment);
    gtk_container_add(GTK_CONTAINER(m_widget), eventbox);

    m_gdkDecor = 0;
    m_gdkFunc = 0;
    if (style & wxRESIZE_BORDER)
       m_gdkFunc = GDK_FUNC_RESIZE;
    gtk_window_set_default_size(GTK_WINDOW(m_widget), m_width, m_height);
    m_decorSize.Set(0, 0);
    m_deferShow = false;

    // don't allow sizing smaller than decorations
    GdkGeometry geom;
    geom.min_width  = 2 * m_miniEdge;
    geom.min_height = 2 * m_miniEdge + m_miniTitle;
    gtk_window_set_geometry_hints(GTK_WINDOW(m_widget), NULL, &geom, GDK_HINT_MIN_SIZE);

    if (m_parent && (GTK_IS_WINDOW(m_parent->m_widget)))
    {
        gtk_window_set_transient_for( GTK_WINDOW(m_widget), GTK_WINDOW(m_parent->m_widget) );
    }

    if (m_miniTitle && (style & wxCLOSE_BOX))
    {
        wxImage img = wxBitmap((const char*)close_bits, 16, 16).ConvertToImage();
        img.Replace(0,0,0,123,123,123);
        img.SetMaskColour(123,123,123);
        m_closeButton = wxBitmap( img );
    }

    /* these are called when the borders are drawn */
    g_signal_connect_after(eventbox, "expose_event",
                      G_CALLBACK (gtk_window_own_expose_callback), this );

    /* these are required for dragging the mini frame around */
    g_signal_connect (eventbox, "button_press_event",
                      G_CALLBACK (gtk_window_button_press_callback), this);
    g_signal_connect (eventbox, "button_release_event",
                      G_CALLBACK (gtk_window_button_release_callback), this);
    g_signal_connect (eventbox, "motion_notify_event",
                      G_CALLBACK (gtk_window_motion_notify_callback), this);
    g_signal_connect (eventbox, "leave_notify_event",
                      G_CALLBACK (gtk_window_leave_callback), this);
    return true;
}

void wxMiniFrame::DoGetClientSize(int* width, int* height) const
{
    wxFrame::DoGetClientSize(width, height);
    if (width)
    {
        *width -= 2 * m_miniEdge;
        if (*width < 0) *width = 0;
    }
    if (height)
    {
        *height -= m_miniTitle + 2 * m_miniEdge;
        if (*height < 0) *height = 0;
    }
}

// Keep min size at least as large as decorations
void wxMiniFrame::DoSetSizeHints(int minW, int minH, int maxW, int maxH, int incW, int incH)
{
    const int w = 2 * m_miniEdge;
    const int h = 2 * m_miniEdge + m_miniTitle;
    if (minW < w) minW = w;
    if (minH < h) minH = h;
    wxFrame::DoSetSizeHints(minW, minH, maxW, maxH, incW, incH);
}

void wxMiniFrame::SetTitle( const wxString &title )
{
    wxFrame::SetTitle( title );

    GtkWidget* widget = gtk_bin_get_child(GTK_BIN(m_widget));
    if (gtk_widget_get_window(widget))
        gdk_window_invalidate_rect(gtk_widget_get_window(widget), NULL, false);
}

#endif // wxUSE_MINIFRAME
