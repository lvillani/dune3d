#include "dune3d_application.hpp"
#include "dune3d_appwindow.hpp"
#include "canvas/canvas.hpp"
#include "widgets/axes_lollipop.hpp"
#include "util/fs_util.hpp"

namespace dune3d {

Dune3DAppWindow *Dune3DAppWindow::create(Dune3DApplication &app)
{
    // Load the Builder file and instantiate its widgets.

    auto refBuilder = Gtk::Builder::create_from_resource("/org/dune3d/dune3d/window.ui");

    auto window = Gtk::Builder::get_widget_derived<Dune3DAppWindow>(refBuilder, "window", app);

    if (!window)
        throw std::runtime_error("No \"window\" object in window.ui");
    return window;
}

Dune3DAppWindow::~Dune3DAppWindow() = default;

Dune3DAppWindow::Dune3DAppWindow(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &refBuilder,
                                 class Dune3DApplication &app)
    : Gtk::ApplicationWindow(cobject), m_editor(*this, app.get_preferences())
{
    m_canvas = Gtk::make_managed<Canvas>();

    {
        auto paned = refBuilder->get_widget<Gtk::Paned>("paned");
        paned->set_shrink_start_child(false);
    }

    m_left_bar = refBuilder->get_widget<Gtk::Paned>("left_bar");


    // auto pick_button = refBuilder->get_widget<Gtk::Button>("pick_button");
    // pick_button->signal_clicked().connect([this] { get_canvas().queue_pick(); });

    m_header_bar = refBuilder->get_widget<Gtk::HeaderBar>("titlebar");

    m_open_button = refBuilder->get_widget<Gtk::Button>("open_button");
    m_new_button = refBuilder->get_widget<Gtk::Button>("new_button");
    m_save_button = refBuilder->get_widget<Gtk::Button>("save_button");
    m_save_as_button = refBuilder->get_widget<Gtk::Button>("save_as_button");


    m_hamburger_menu_button = refBuilder->get_widget<Gtk::MenuButton>("hamburger_menu_button");


    m_tool_bar = refBuilder->get_widget<Gtk::Revealer>("tool_bar");
    m_tool_bar_actions_box = refBuilder->get_widget<Gtk::Box>("tool_bar_actions_box");
    m_tool_bar_box = refBuilder->get_widget<Gtk::Box>("tool_bar_box");
    m_tool_bar_name_label = refBuilder->get_widget<Gtk::Label>("tool_bar_name_label");
    m_tool_bar_tip_label = refBuilder->get_widget<Gtk::Label>("tool_bar_tip_label");
    m_tool_bar_flash_label = refBuilder->get_widget<Gtk::Label>("tool_bar_flash_label");
    m_tool_bar_stack = refBuilder->get_widget<Gtk::Stack>("tool_bar_stack");
    m_tool_bar_stack->set_visible_child(*m_tool_bar_box);

    refBuilder->get_widget<Gtk::Box>("canvas_box")->insert_child_at_start(*m_canvas);
    get_canvas().set_vexpand(true);
    get_canvas().set_hexpand(true);
    m_key_hint_label = refBuilder->get_widget<Gtk::Label>("key_hint_label");
    m_workplane_label = refBuilder->get_widget<Gtk::Label>("workplane_label");


    {
        Gtk::Box *lollipop_box = refBuilder->get_widget<Gtk::Box>("lollipop_box");
        auto axes_lollipop = Gtk::make_managed<AxesLollipop>();
        lollipop_box->append(*axes_lollipop);
        get_canvas().signal_view_changed().connect(sigc::track_obj(
                [this, axes_lollipop] {
                    const float alpha = -glm::radians(get_canvas().get_cam_azimuth() + 90);
                    const float beta = glm::radians(get_canvas().get_cam_elevation() - 90);
                    axes_lollipop->set_angles(alpha, beta);
                },
                *axes_lollipop));
    }
    get_canvas().set_cam_azimuth(270);
    get_canvas().set_cam_elevation(80);


    m_version_info_bar = refBuilder->get_widget<Gtk::InfoBar>("version_info_bar");
    m_version_info_bar_label = refBuilder->get_widget<Gtk::Label>("version_info_bar_label");

    m_selection_mode_label = refBuilder->get_widget<Gtk::Label>("selection_mode_label");

    set_icon_name("dune3d");

    m_editor.init();
}

void Dune3DAppWindow::set_workplane_label_text(const std::string &s)
{
    m_workplane_label->set_text(s);
}

void Dune3DAppWindow::set_key_hint_label_text(const std::string &s)
{
    m_key_hint_label->set_text(s);
}

void Dune3DAppWindow::set_selection_mode_label_text(const std::string &s)
{
    m_selection_mode_label->set_text(s);
}


void Dune3DAppWindow::tool_bar_set_visible(bool v)
{
    if (v == false && m_tip_timeout_connection) { // hide and tip is still shown
        m_tool_bar_queue_close = true;
    }
    else {
        m_tool_bar->set_reveal_child(v);
        if (v) {
            m_tool_bar_queue_close = false;
        }
    }
}

void Dune3DAppWindow::tool_bar_set_tool_name(const std::string &s)
{
    m_tool_bar_name_label->set_text(s);
}

void Dune3DAppWindow::tool_bar_set_tool_tip(const std::string &s)
{
    if (s.size()) {
        m_tool_bar_tip_label->set_markup(s);
        m_tool_bar_tip_label->show();
    }
    else {
        m_tool_bar_tip_label->hide();
    }
}

void Dune3DAppWindow::tool_bar_flash(const std::string &s)
{
    tool_bar_flash(s, false);
}

void Dune3DAppWindow::tool_bar_flash_replace(const std::string &s)
{
    tool_bar_flash(s, true);
}

void Dune3DAppWindow::tool_bar_flash(const std::string &s, bool replace)
{
    if (m_flash_text.size() && !replace)
        m_flash_text += "; " + s;
    else
        m_flash_text = s;

    m_tool_bar_flash_label->set_markup(m_flash_text);

    m_tool_bar_stack->set_visible_child(*m_tool_bar_flash_label);
    m_tip_timeout_connection.disconnect();
    m_tip_timeout_connection = Glib::signal_timeout().connect(
            [this] {
                m_tool_bar_stack->set_visible_child(*m_tool_bar_box);

                m_flash_text.clear();
                if (m_tool_bar_queue_close)
                    m_tool_bar->set_reveal_child(false);
                m_tool_bar_queue_close = false;
                return false;
            },
            2000);
}

void Dune3DAppWindow::tool_bar_set_vertical(bool v)
{
    m_tool_bar_box->set_orientation(v ? Gtk::Orientation::VERTICAL : Gtk::Orientation::HORIZONTAL);
}

void Dune3DAppWindow::tool_bar_append_action(Gtk::Widget &w)
{
    m_tool_bar_actions_box->append(w);
    m_action_widgets.push_back(&w);
    m_tool_bar_actions_box->show();
}

void Dune3DAppWindow::tool_bar_clear_actions()
{
    for (auto w : m_action_widgets) {
        m_tool_bar_actions_box->remove(*w);
    }
    m_action_widgets.clear();
}


void Dune3DAppWindow::set_version_info(const std::string &s)
{
    if (s.size()) {
        m_version_info_bar->set_revealed(true);
        m_version_info_bar_label->set_markup(s);
    }
    else {
        m_version_info_bar->set_revealed(false);
    }
}

void Dune3DAppWindow::open_file_view(const Glib::RefPtr<Gio::File> &file)
{
    m_editor.open_file(path_from_string(file->get_path()));
}


} // namespace dune3d
