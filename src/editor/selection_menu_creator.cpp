#include "selection_menu_creator.hpp"
#include "canvas/selectable_checkbutton.hpp"
#include "core/core.hpp"
#include "document/entity/entity.hpp"
#include "document/constraint/constraint.hpp"
#include "document/constraint/iconstraint_datum.hpp"
#include "document/group/group.hpp"

namespace dune3d {

SelectionMenuCreator::SelectionMenuCreator(Core &core) : m_core(core)
{
}

std::vector<SelectableCheckButton *> SelectionMenuCreator::create(Gtk::Popover &popover,
                                                                  const SelectableRefAndVertexTypeList &list)
{
    auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);

    std::vector<SelectableCheckButton *> buttons;
    std::optional<ICanvas::VertexType> first_vertex_type;
    Gtk::Box *more_box = nullptr;
    for (const auto &it : list) {
        if (it.selectable.has_value()) {
            if (!first_vertex_type.has_value())
                first_vertex_type = it.vertex_type;
            std::string label;
            switch (it.selectable->type) {
            case SelectableRef::Type::ENTITY: {
                auto &doc = m_core.get_current_document();
                auto &entity = doc.get_entity(it.selectable->item);
                auto &group = doc.get_group(entity.m_group);
                if (entity.m_construction)
                    label = "Construction ";
                label += entity.get_type_name();
                if (entity.has_name() && entity.m_name.size())
                    label += " " + entity.m_name;
                if (auto point_name = entity.get_point_name(it.selectable->point); point_name.size())
                    label += " (" + point_name + ")";
                label += " in group " + group.m_name;
            } break;

            case SelectableRef::Type::CONSTRAINT: {
                auto &doc = m_core.get_current_document();
                auto &constraint = doc.get_constraint(it.selectable->item);
                auto &group = doc.get_group(constraint.m_group);

                std::string name = "constraint";
                if (auto dat = dynamic_cast<const IConstraintDatum *>(&constraint); dat && dat->is_measurement())
                    name = "measurement";

                label = constraint.get_type_name() + " " + name + " in group " + group.m_name;
            } break;

            case SelectableRef::Type::DOCUMENT: {
                label = "Document " + m_core.get_idocument_info(it.selectable->item).get_basename();
            } break;

            case SelectableRef::Type::SOLID_MODEL_EDGE: {
                label = "Solid model edge";
            } break;
            }
            auto cb = Gtk::make_managed<SelectableCheckButton>(label);
            cb->m_selectable = it.selectable.value();
            buttons.push_back(cb);
            if (it.vertex_type == first_vertex_type.value()) {
                box->append(*cb);
            }
            else {
                if (!more_box)
                    more_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
                more_box->append(*cb);
            }
        }
    }
    if (more_box) {
        auto more_link = Gtk::make_managed<Gtk::Label>();
        more_link->set_margin_start(4);
        more_link->set_xalign(0);
        more_link->set_markup("<a href=\"\">Show more…</a>");
        box->append(*more_link);
        box->append(*more_box);
        more_box->hide();
        more_link->signal_activate_link().connect(
                [more_link, more_box, &popover](const auto &link) {
                    more_link->hide();
                    more_box->show();
                    popover.popdown();
                    popover.popup();
                    return true;
                },
                false);
    }
    popover.set_child(*box);

    return buttons;
}


} // namespace dune3d
