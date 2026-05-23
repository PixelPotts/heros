#include "window.h"
#include "ui.h"
#include "animation.h"
#include <algorithm>
#include <cstdio>

// ── Colors (match ui.cpp palette) ───────────────────────────────

static const SDL_Color WHITE  = {230, 230, 240, 255};
static const SDL_Color DIM    = {150, 160, 180, 255};
static const SDL_Color ACCENT = {100, 150, 255, 255};

// ── Layout constants ────────────────────────────────────────────

static const int TOPBAR_H = 36;
static const int DOCK_H   = 58;

// ── Window::hit_test ────────────────────────────────────────────

HitZone Window::hit_test(int mx, int my) const {
    // Outside window entirely?
    if (mx < rect.x || mx >= rect.x + rect.w ||
        my < rect.y || my >= rect.y + rect.h)
        return HitZone::None;

    // Check resize edges first (only if resizable and not maximized)
    if ((flags & WF_Resizable) && !maximized) {
        bool top    = (my < rect.y + RESIZE_EDGE);
        bool bottom = (my >= rect.y + rect.h - RESIZE_EDGE);
        bool left   = (mx < rect.x + RESIZE_EDGE);
        bool right  = (mx >= rect.x + rect.w - RESIZE_EDGE);

        if (top && left)   return HitZone::ResizeNW;
        if (top && right)  return HitZone::ResizeNE;
        if (bottom && left)  return HitZone::ResizeSW;
        if (bottom && right) return HitZone::ResizeSE;
        if (top)    return HitZone::ResizeN;
        if (bottom) return HitZone::ResizeS;
        if (left)   return HitZone::ResizeW;
        if (right)  return HitZone::ResizeE;
    }

    // Title bar area
    if (my < rect.y + TITLEBAR_H) {
        // Check buttons (right to left: close, maximize, minimize)
        SDL_Rect cb = close_btn_rect();
        if (mx >= cb.x - 4 && mx <= cb.x + cb.w + 4 &&
            my >= cb.y - 4 && my <= cb.y + cb.h + 4)
            return HitZone::CloseButton;

        SDL_Rect mb = maximize_btn_rect();
        if (mx >= mb.x - 4 && mx <= mb.x + mb.w + 4 &&
            my >= mb.y - 4 && my <= mb.y + mb.h + 4)
            return HitZone::MaximizeButton;

        SDL_Rect nb = minimize_btn_rect();
        if (mx >= nb.x - 4 && mx <= nb.x + nb.w + 4 &&
            my >= nb.y - 4 && my <= nb.y + nb.h + 4)
            return HitZone::MinimizeButton;

        return HitZone::TitleBar;
    }

    return HitZone::Content;
}

// ── WindowManager — lifecycle ───────────────────────────────────

int WindowManager::open_window(const std::string& title, Icon icon, SDL_Rect rect,
                               uint32_t flags, std::unique_ptr<AppContent> content) {
    Window w;
    w.id = next_id_++;
    w.title = title;
    w.icon = icon;
    w.rect = rect;
    w.restore_rect = rect;
    w.flags = flags;
    w.content = std::move(content);
    w.z_order = next_z_++;
    w.active = true;
    w.visible = true;

    // Deactivate all others and notify
    for (auto& win : windows_) {
        if (win.active && win.content) win.content->on_deactivate();
        win.active = false;
    }

    windows_.push_back(std::move(w));
    int new_id = windows_.back().id;
    // Notify new window it's active
    if (windows_.back().content) windows_.back().content->on_activate();
    // Trigger open animation
    if (animations_) animations_->animate_open(new_id, windows_.back().rect);
    return new_id;
}

void WindowManager::close_window(int id) {
    // Ask app if it's OK to close
    Window* w = find_window(id);
    if (w && w->content && !w->content->on_close()) {
        return; // App vetoed the close
    }

    // Remove from minimized list
    minimized_ids_.erase(
        std::remove(minimized_ids_.begin(), minimized_ids_.end(), id),
        minimized_ids_.end());

    // Remove from windows
    windows_.erase(
        std::remove_if(windows_.begin(), windows_.end(),
            [id](const Window& w) { return w.id == id; }),
        windows_.end());

    focus_next_topmost();
}

Window* WindowManager::find_window(int id) {
    for (auto& w : windows_)
        if (w.id == id) return &w;
    return nullptr;
}

const Window* WindowManager::find_window(int id) const {
    for (auto& w : windows_)
        if (w.id == id) return &w;
    return nullptr;
}

Window* WindowManager::focused_window() {
    for (auto& w : windows_)
        if (w.active && !w.minimized) return &w;
    return nullptr;
}

bool WindowManager::is_minimized(int id) const {
    return std::find(minimized_ids_.begin(), minimized_ids_.end(), id) != minimized_ids_.end();
}

// ── WindowManager — window operations ───────────────────────────

void WindowManager::bring_to_front(int id) {
    Window* w = find_window(id);
    if (w) w->z_order = next_z_++;
}

void WindowManager::set_focus(int id) {
    for (auto& w : windows_) {
        bool was_active = w.active;
        w.active = (w.id == id);
        if (w.content) {
            if (!was_active && w.active) w.content->on_activate();
            if (was_active && !w.active) w.content->on_deactivate();
        }
    }
}

void WindowManager::minimize(int id) {
    Window* w = find_window(id);
    if (!w || !(w->flags & WF_Minimizable)) return;
    if (w->active && w->content) w->content->on_deactivate();
    w->minimized = true;
    w->visible = false;
    w->active = false;
    if (std::find(minimized_ids_.begin(), minimized_ids_.end(), id) == minimized_ids_.end())
        minimized_ids_.push_back(id);
    focus_next_topmost();
}

void WindowManager::maximize(int id, int screen_w, int screen_h) {
    Window* w = find_window(id);
    if (!w || !(w->flags & WF_Maximizable)) return;
    w->restore_rect = w->rect;
    w->rect = {0, TOPBAR_H, screen_w, screen_h - TOPBAR_H - DOCK_H};
    w->maximized = true;
    if (w->content) {
        SDL_Rect cr = w->content_rect();
        w->content->on_resize(cr.w, cr.h);
    }
}

void WindowManager::restore(int id) {
    Window* w = find_window(id);
    if (!w) return;
    w->rect = w->restore_rect;
    w->maximized = false;
    if (w->content) {
        SDL_Rect cr = w->content_rect();
        w->content->on_resize(cr.w, cr.h);
    }
}

void WindowManager::toggle_maximize(int id, int screen_w, int screen_h) {
    Window* w = find_window(id);
    if (!w) return;
    if (w->maximized) restore(id);
    else maximize(id, screen_w, screen_h);
}

void WindowManager::restore_from_dock(int id, int screen_w, int screen_h) {
    (void)screen_w; (void)screen_h;
    Window* w = find_window(id);
    if (!w) return;
    w->minimized = false;
    w->visible = true;
    minimized_ids_.erase(
        std::remove(minimized_ids_.begin(), minimized_ids_.end(), id),
        minimized_ids_.end());
    bring_to_front(id);
    set_focus(id);
}

// ── Window snapping ──────────────────────────────────────────────

static const int SNAP_EDGE = 12;

SnapZone WindowManager::detect_snap_zone(int mx, int my) const {
    bool at_left   = (mx <= SNAP_EDGE);
    bool at_right  = (mx >= screen_w_ - SNAP_EDGE);
    bool at_top    = (my <= TOPBAR_H + SNAP_EDGE);
    bool at_bottom = (my >= screen_h_ - DOCK_H - SNAP_EDGE);

    if (at_top && at_left)     return SnapZone::TopLeft;
    if (at_top && at_right)    return SnapZone::TopRight;
    if (at_bottom && at_left)  return SnapZone::BottomLeft;
    if (at_bottom && at_right) return SnapZone::BottomRight;
    if (at_top)    return SnapZone::Maximize;
    if (at_left)   return SnapZone::Left;
    if (at_right)  return SnapZone::Right;
    return SnapZone::None;
}

SDL_Rect WindowManager::snap_zone_rect(SnapZone zone) const {
    int usable_h = screen_h_ - TOPBAR_H - DOCK_H;
    int half_w = screen_w_ / 2;
    int half_h = usable_h / 2;

    switch (zone) {
    case SnapZone::Left:        return {0, TOPBAR_H, half_w, usable_h};
    case SnapZone::Right:       return {half_w, TOPBAR_H, screen_w_ - half_w, usable_h};
    case SnapZone::TopLeft:     return {0, TOPBAR_H, half_w, half_h};
    case SnapZone::TopRight:    return {half_w, TOPBAR_H, screen_w_ - half_w, half_h};
    case SnapZone::BottomLeft:  return {0, TOPBAR_H + half_h, half_w, usable_h - half_h};
    case SnapZone::BottomRight: return {half_w, TOPBAR_H + half_h, screen_w_ - half_w, usable_h - half_h};
    case SnapZone::Maximize:    return {0, TOPBAR_H, screen_w_, usable_h};
    default:                    return {0, 0, 0, 0};
    }
}

void WindowManager::snap_window(int id, SnapZone zone, int screen_w, int screen_h) {
    Window* w = find_window(id);
    if (!w) return;

    screen_w_ = screen_w;
    screen_h_ = screen_h;

    if (zone == SnapZone::None) return;
    if (zone == SnapZone::Maximize) {
        maximize(id, screen_w, screen_h);
        return;
    }

    // Save restore rect if not already snapped/maximized
    if (!w->maximized) {
        w->restore_rect = w->rect;
    }
    w->rect = snap_zone_rect(zone);
    w->maximized = false; // snapped, not maximized
    if (w->content) {
        SDL_Rect cr = w->content_rect();
        w->content->on_resize(cr.w, cr.h);
    }
}

void WindowManager::snap_left(int id, int screen_w, int screen_h) {
    snap_window(id, SnapZone::Left, screen_w, screen_h);
}

void WindowManager::snap_right(int id, int screen_w, int screen_h) {
    snap_window(id, SnapZone::Right, screen_w, screen_h);
}

void WindowManager::render_snap_preview(SDL_Renderer* r) {
    if (snap_preview_ == SnapZone::None) return;

    SDL_Rect zone = snap_zone_rect(snap_preview_);
    // Inset slightly for visual padding
    zone.x += 4; zone.y += 4;
    zone.w -= 8; zone.h -= 8;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    draw::filled_rounded_rect(r, zone, 12, {100, 150, 255, 35});
    draw::rounded_rect(r, zone, 12, {100, 150, 255, 100});
}

void WindowManager::focus_next_topmost() {
    Window* top = nullptr;
    for (auto& w : windows_) {
        if (!w.minimized && w.visible) {
            if (!top || w.z_order > top->z_order) top = &w;
        }
    }
    if (top) set_focus(top->id);
}

// ── WindowManager — z-order sorting ─────────────────────────────

std::vector<Window*> WindowManager::z_sorted() {
    std::vector<Window*> sorted;
    for (auto& w : windows_) {
        if (!w.minimized && w.visible) sorted.push_back(&w);
    }
    std::sort(sorted.begin(), sorted.end(),
        [](const Window* a, const Window* b) { return a->z_order < b->z_order; });
    return sorted;
}

// ── WindowManager — find window at point ────────────────────────

Window* WindowManager::window_at(int mx, int my) {
    // Search in reverse z-order (topmost first)
    auto sorted = z_sorted();
    for (int i = (int)sorted.size() - 1; i >= 0; i--) {
        if (sorted[i]->hit_test(mx, my) != HitZone::None)
            return sorted[i];
    }
    return nullptr;
}

// ── WindowManager — event routing ───────────────────────────────

bool WindowManager::handle_event(const SDL_Event& event) {
    switch (event.type) {
    case SDL_MOUSEBUTTONDOWN:
        if (event.button.button == SDL_BUTTON_LEFT) {
            on_mouse_down(event.button.x, event.button.y, screen_w_, screen_h_);
            return true;
        }
        break;
    case SDL_MOUSEBUTTONUP:
        if (event.button.button == SDL_BUTTON_LEFT) {
            on_mouse_up(event.button.x, event.button.y);
            return true;
        }
        break;
    case SDL_MOUSEMOTION:
        on_mouse_move(event.motion.x, event.motion.y, screen_w_, screen_h_);
        break;
    case SDL_TEXTINPUT:
        {
            Window* fw = focused_window();
            if (fw && fw->content)
                fw->content->on_text_input(event.text.text);
        }
        break;
    case SDL_MOUSEWHEEL:
        {
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            Window* sw = window_at(mx, my);
            if (sw && sw->content) {
                SDL_Rect cr = sw->content_rect();
                sw->content->on_scroll(mx - cr.x, my - cr.y, event.wheel.y);
            }
        }
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        on_key_event(event);
        break;
    case SDL_WINDOWEVENT:
        if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            screen_w_ = event.window.data1;
            screen_h_ = event.window.data2;
        }
        break;
    default:
        break;
    }
    return false;
}

// ── Mouse down ──────────────────────────────────────────────────

void WindowManager::on_mouse_down(int mx, int my, int screen_w, int screen_h) {
    Window* w = window_at(mx, my);
    if (!w) return;

    bring_to_front(w->id);
    set_focus(w->id);

    HitZone zone = w->hit_test(mx, my);
    switch (zone) {
    case HitZone::CloseButton:
        if (w->flags & WF_Closable) close_window(w->id);
        return;
    case HitZone::MinimizeButton:
        if (w->flags & WF_Minimizable) minimize(w->id);
        return;
    case HitZone::MaximizeButton:
        if (w->flags & WF_Maximizable) toggle_maximize(w->id, screen_w, screen_h);
        return;
    case HitZone::TitleBar:
        if (w->flags & WF_Draggable) {
            // If maximized, un-maximize and reposition so cursor is proportional
            if (w->maximized) {
                float ratio = (float)(mx - w->rect.x) / w->rect.w;
                restore(w->id);
                w->rect.x = mx - (int)(w->rect.w * ratio);
                w->rect.y = my - TITLEBAR_H / 2;
            }
            drag_mode_ = DragMode::Move;
            drag_win_id_ = w->id;
            drag_offset_x_ = mx - w->rect.x;
            drag_offset_y_ = my - w->rect.y;
            drag_start_rect_ = w->rect;
        }
        return;
    case HitZone::Content:
        if (w->content) {
            SDL_Rect cr = w->content_rect();
            w->content->on_mouse_down(mx - cr.x, my - cr.y);
        }
        return;
    default:
        // Resize zones
        if (w->flags & WF_Resizable) {
            drag_win_id_ = w->id;
            drag_start_rect_ = w->rect;
            drag_offset_x_ = mx;
            drag_offset_y_ = my;
            switch (zone) {
            case HitZone::ResizeN:  drag_mode_ = DragMode::ResizeN; break;
            case HitZone::ResizeS:  drag_mode_ = DragMode::ResizeS; break;
            case HitZone::ResizeE:  drag_mode_ = DragMode::ResizeE; break;
            case HitZone::ResizeW:  drag_mode_ = DragMode::ResizeW; break;
            case HitZone::ResizeNE: drag_mode_ = DragMode::ResizeNE; break;
            case HitZone::ResizeNW: drag_mode_ = DragMode::ResizeNW; break;
            case HitZone::ResizeSE: drag_mode_ = DragMode::ResizeSE; break;
            case HitZone::ResizeSW: drag_mode_ = DragMode::ResizeSW; break;
            default: break;
            }
        }
        return;
    }
}

// ── Mouse move ──────────────────────────────────────────────────

void WindowManager::on_mouse_move(int mx, int my, int screen_w, int screen_h) {
    // Handle active drag/resize
    if (drag_mode_ != DragMode::None) {
        Window* w = find_window(drag_win_id_);
        if (!w) { drag_mode_ = DragMode::None; return; }

        if (drag_mode_ == DragMode::Move) {
            w->rect.x = mx - drag_offset_x_;
            w->rect.y = my - drag_offset_y_;
            // Constrain: title bar stays on screen
            w->rect.y = std::max(TOPBAR_H, std::min(w->rect.y, screen_h - TITLEBAR_H));
            w->rect.x = std::max(-(w->rect.w - 40), std::min(w->rect.x, screen_w - 40));
            // Show snap preview
            snap_preview_ = detect_snap_zone(mx, my);
        } else {
            int dx = mx - drag_offset_x_;
            int dy = my - drag_offset_y_;
            SDL_Rect r = drag_start_rect_;

            bool resize_n = (drag_mode_ == DragMode::ResizeN || drag_mode_ == DragMode::ResizeNE || drag_mode_ == DragMode::ResizeNW);
            bool resize_s = (drag_mode_ == DragMode::ResizeS || drag_mode_ == DragMode::ResizeSE || drag_mode_ == DragMode::ResizeSW);
            bool resize_w = (drag_mode_ == DragMode::ResizeW || drag_mode_ == DragMode::ResizeNW || drag_mode_ == DragMode::ResizeSW);
            bool resize_e = (drag_mode_ == DragMode::ResizeE || drag_mode_ == DragMode::ResizeNE || drag_mode_ == DragMode::ResizeSE);

            if (resize_n) { r.y += dy; r.h -= dy; }
            if (resize_s) { r.h += dy; }
            if (resize_w) { r.x += dx; r.w -= dx; }
            if (resize_e) { r.w += dx; }

            // Enforce minimum size
            if (r.w < w->min_w) {
                if (resize_w) r.x = drag_start_rect_.x + drag_start_rect_.w - w->min_w;
                r.w = w->min_w;
            }
            if (r.h < w->min_h) {
                if (resize_n) r.y = drag_start_rect_.y + drag_start_rect_.h - w->min_h;
                r.h = w->min_h;
            }

            w->rect = r;
        }
        return;
    }

    // Update hover state
    Window* w = window_at(mx, my);
    if (w) {
        hover_win_id_ = w->id;
        hover_zone_ = w->hit_test(mx, my);

        // Route to content
        if (hover_zone_ == HitZone::Content && w->content) {
            SDL_Rect cr = w->content_rect();
            w->content->on_mouse_move(mx - cr.x, my - cr.y);
        }
    } else {
        hover_win_id_ = 0;
        hover_zone_ = HitZone::None;
    }
}

// ── Mouse up ────────────────────────────────────────────────────

void WindowManager::on_mouse_up(int mx, int my) {
    if (drag_mode_ != DragMode::None) {
        if (drag_mode_ == DragMode::Move && snap_preview_ != SnapZone::None) {
            // Apply snap
            snap_window(drag_win_id_, snap_preview_, screen_w_, screen_h_);
            snap_preview_ = SnapZone::None;
        } else if (drag_mode_ != DragMode::Move) {
            // If we were resizing, notify app of new size
            Window* w = find_window(drag_win_id_);
            if (w && w->content) {
                SDL_Rect cr = w->content_rect();
                w->content->on_resize(cr.w, cr.h);
            }
        }
        snap_preview_ = SnapZone::None;
        drag_mode_ = DragMode::None;
        drag_win_id_ = 0;
        return;
    }

    // Route to content
    Window* w = window_at(mx, my);
    if (w && w->content) {
        HitZone zone = w->hit_test(mx, my);
        if (zone == HitZone::Content) {
            SDL_Rect cr = w->content_rect();
            w->content->on_mouse_up(mx - cr.x, my - cr.y);
        }
    }
}

// ── Key events ──────────────────────────────────────────────────

void WindowManager::on_key_event(const SDL_Event& event) {
    Window* w = focused_window();
    if (!w || !w->content) return;

    if (event.type == SDL_KEYDOWN)
        w->content->on_key_down(event.key.keysym.sym);
    else if (event.type == SDL_KEYUP)
        w->content->on_key_up(event.key.keysym.sym);
}

// ── Rendering ───────────────────────────────────────────────────

void WindowManager::render(const RenderCtx& ctx) {
    screen_w_ = ctx.w;
    screen_h_ = ctx.h;

    // Render snap preview zone highlight
    render_snap_preview(ctx.r);

    auto sorted = z_sorted();

    for (Window* w : sorted) {
        SDL_Renderer* r = ctx.r;

        // Check for animation override
        SDL_Rect render_rect = w->rect;
        float anim_alpha = 1.0f;
        float anim_scale = 1.0f;
        bool animating = false;

        if (ctx.animations && ctx.animations->is_animating(w->id)) {
            animating = ctx.animations->get_animated_state(w->id, render_rect, anim_alpha, anim_scale);
            if (anim_alpha <= 0.01f) continue; // fully transparent, skip render
        }

        // Use animated rect for rendering
        SDL_Rect orig_rect = w->rect;
        if (animating) w->rect = render_rect;

        // Apply alpha modulation for animations
        Uint8 alpha_mod = (Uint8)(anim_alpha * 255);
        (void)alpha_mod; // Used conceptually — SDL2 doesn't have per-primitive alpha easily

        // Shadow
        SDL_Rect shadow = {w->rect.x + 4, w->rect.y + 4, w->rect.w, w->rect.h};
        Uint8 shadow_a = (Uint8)(40 * anim_alpha);
        draw::filled_rounded_rect(r, shadow, 10, {0, 0, 0, shadow_a});

        // Frost panel body
        ctx.frost->render_panel(r, w->rect, {10, 14, 25, 170});
        draw::rounded_rect(r, w->rect, 10, {180, 195, 220, 40});

        // Title bar
        int tby = w->rect.y + 4;
        draw::line(r, w->rect.x, w->rect.y + TITLEBAR_H,
                   w->rect.x + w->rect.w, w->rect.y + TITLEBAR_H, {180, 195, 220, 25});
        draw::text(r, ctx.fonts->body, w->title.c_str(),
                   w->rect.x + 14, tby + 6, WHITE);

        // Title bar buttons with hover highlights
        bool this_hover = (hover_win_id_ == w->id);

        // Close button
        {
            SDL_Rect cb = w->close_btn_rect();
            int cx = cb.x + cb.w / 2;
            int cy = cb.y + cb.h / 2;
            bool hovered = this_hover && hover_zone_ == HitZone::CloseButton;
            if (hovered) {
                draw::filled_rounded_rect(r, {cb.x - 4, cb.y - 4, cb.w + 8, cb.h + 8}, 4, {200, 60, 60, 60});
            }
            draw::icon(r, Icon::Close, cx, cy, BTN_SIZE, hovered ? SDL_Color{255, 120, 120, 255} : SDL_Color{200, 100, 100, 200});
        }

        // Maximize button
        {
            SDL_Rect mb = w->maximize_btn_rect();
            int cx = mb.x + mb.w / 2;
            int cy = mb.y + mb.h / 2;
            bool hovered = this_hover && hover_zone_ == HitZone::MaximizeButton;
            if (hovered) {
                draw::filled_rounded_rect(r, {mb.x - 4, mb.y - 4, mb.w + 8, mb.h + 8}, 4, {100, 150, 255, 40});
            }
            draw::icon(r, Icon::Maximize, cx, cy, BTN_SIZE, hovered ? ACCENT : DIM);
        }

        // Minimize button
        {
            SDL_Rect nb = w->minimize_btn_rect();
            int cx = nb.x + nb.w / 2;
            int cy = nb.y + nb.h / 2;
            bool hovered = this_hover && hover_zone_ == HitZone::MinimizeButton;
            if (hovered) {
                draw::filled_rounded_rect(r, {nb.x - 4, nb.y - 4, nb.w + 8, nb.h + 8}, 4, {100, 150, 255, 40});
            }
            draw::icon(r, Icon::Minimize, cx, cy, BTN_SIZE, hovered ? ACCENT : DIM);
        }

        // Content — clip to content area
        SDL_Rect cr = w->content_rect();
        SDL_RenderSetClipRect(r, &cr);
        if (w->content) {
            w->content->render(ctx, cr);
        }
        SDL_RenderSetClipRect(r, nullptr);

        // Restore original rect if animated
        if (animating) w->rect = orig_rect;
    }
}
