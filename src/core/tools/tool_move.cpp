#include "tool_move.hpp"
#include "document/document.hpp"
#include "document/group/group.hpp"
#include "document/entity/entity_line3d.hpp"
#include "document/entity/entity_line2d.hpp"
#include "document/entity/entity_arc2d.hpp"
#include "document/entity/entity_arc3d.hpp"
#include "document/entity/entity_circle2d.hpp"
#include "document/entity/entity_circle3d.hpp"
#include "document/entity/entity_workplane.hpp"
#include "document/entity/entity_step.hpp"
#include "document/entity/entity_point2d.hpp"
#include "document/entity/entity_document.hpp"
#include "document/constraint/constraint_point_distance.hpp"
#include "document/constraint/constraint_diameter_radius.hpp"
#include "document/constraint/constraint_angle.hpp"
#include "document/constraint/constraint_point_line_distance.hpp"
#include "editor/editor_interface.hpp"
#include "tool_common_impl.hpp"

namespace dune3d {

ToolBase::CanBegin ToolMove::can_begin()
{
    for (const auto &sr : m_selection) {
        if (sr.type == SelectableRef::Type::ENTITY) {
            auto &entity = get_entity(sr.item);
            if (entity.can_move(get_doc()))
                return true;
        }
        else if (sr.type == SelectableRef::Type::CONSTRAINT) {
            auto &constr = get_doc().get_constraint(sr.item);
            if (dynamic_cast<const IConstraintMovable *>(&constr))
                return true;
        }
    }
    return false;
}

ToolResponse ToolMove::begin(const ToolArgs &args)
{
    auto &doc = get_doc();
    m_inital_pos = m_intf.get_cursor_pos();
    for (const auto &[uu, en] : doc.m_entities) {
        if (en->get_type() == Entity::Type::WORKPLANE) {
            auto &wrkpl = dynamic_cast<const EntityWorkplane &>(*en);
            m_inital_pos_wrkpl.emplace(uu, wrkpl.project(get_cursor_pos_for_workplane(wrkpl)));
        }
    }
    const Group *first_group = nullptr;
    for (const auto &sr : m_selection) {
        if (sr.type == SelectableRef::Type::ENTITY) {
            auto &entity = get_entity(sr.item);
            if (entity.m_move_instead.contains(sr.point)) {
                auto &enp = entity.m_move_instead.at(sr.point);
                auto &other_entity = get_entity(enp.entity);
                get_doc().accumulate_first_group(first_group, other_entity.m_group);
                m_entities.emplace(&other_entity, enp.point);
            }
            else {
                get_doc().accumulate_first_group(first_group, entity.m_group);
                m_entities.emplace(&entity, sr.point);
            }
        }
        else if (sr.type == SelectableRef::Type::CONSTRAINT) {
            if (auto constraint = get_doc().get_constraint_ptr<ConstraintLinesAngle>(sr.item)) {
                if (!constraint->m_wrkpl) {
                    auto vecs = constraint->get_vectors(get_doc());
                    m_inital_pos_angle_constraint.emplace(
                            sr.item, m_intf.get_cursor_pos_for_plane(constraint->get_origin(get_doc()), vecs.n));
                }
            }
        }
        // we don't care about constraints since dragging them is pureley cosmetic
    }
    if (first_group)
        m_first_group = first_group->m_uuid;


    for (auto [entity, point] : m_entities) {
        m_dragged_list.emplace_back(entity->m_uuid, point);
    }

    return ToolResponse();
}


ToolResponse ToolMove::update(const ToolArgs &args)
{
    auto &doc = get_doc();
    auto &last_doc = m_core.get_current_last_document();
    if (args.type == ToolEventType::MOVE) {
        const auto delta = m_intf.get_cursor_pos() - m_inital_pos;
        for (auto [entity, point] : m_entities) {
            if (!entity->can_move(doc))
                continue;
            if (auto en = dynamic_cast<EntityLine3D *>(entity)) {
                auto &en_last = dynamic_cast<const EntityLine3D &>(*last_doc.m_entities.at(entity->m_uuid));
                if (point == 0 || point == 1) {
                    en->m_p1 = en_last.m_p1 + delta;
                }
                if (point == 0 || point == 2) {
                    en->m_p2 = en_last.m_p2 + delta;
                }
            }
            if (auto en = dynamic_cast<EntityWorkplane *>(entity)) {
                auto &en_last = dynamic_cast<const EntityWorkplane &>(*last_doc.m_entities.at(entity->m_uuid));
                en->m_origin = en_last.m_origin + delta;
            }
            if (auto en = dynamic_cast<EntitySTEP *>(entity)) {
                auto &en_last = dynamic_cast<const EntitySTEP &>(*last_doc.m_entities.at(entity->m_uuid));
                if (point == 0 || point == 1)
                    en->m_origin = en_last.m_origin + delta;
                else if (en->m_anchors_transformed.contains(point) && en_last.m_anchors_transformed.contains(point))
                    en->m_anchors_transformed.at(point) = en_last.m_anchors_transformed.at(point) + delta;
            }
            if (auto en = dynamic_cast<EntityDocument *>(entity)) {
                auto &en_last = dynamic_cast<const EntityDocument &>(*last_doc.m_entities.at(entity->m_uuid));
                if (point == 0 || point == 1)
                    en->m_origin = en_last.m_origin + delta;
            }
            if (auto en = dynamic_cast<EntityLine2D *>(entity)) {
                auto &en_last = dynamic_cast<const EntityLine2D &>(*last_doc.m_entities.at(entity->m_uuid));
                auto &wrkpl = dynamic_cast<const EntityWorkplane &>(*doc.m_entities.at(en->m_wrkpl));
                const auto delta2d =
                        wrkpl.project(get_cursor_pos_for_workplane(wrkpl)) - m_inital_pos_wrkpl.at(wrkpl.m_uuid);
                if (point == 0 || point == 1) {
                    en->m_p1 = en_last.m_p1 + delta2d;
                }
                if (point == 0 || point == 2) {
                    en->m_p2 = en_last.m_p2 + delta2d;
                }
            }
            if (auto en = dynamic_cast<EntityArc2D *>(entity)) {
                auto &en_last = dynamic_cast<const EntityArc2D &>(*last_doc.m_entities.at(entity->m_uuid));
                auto &wrkpl = dynamic_cast<const EntityWorkplane &>(*doc.m_entities.at(en->m_wrkpl));
                const auto delta2d =
                        wrkpl.project(get_cursor_pos_for_workplane(wrkpl)) - m_inital_pos_wrkpl.at(wrkpl.m_uuid);
                if (point == 0 || point == 1) {
                    en->m_from = en_last.m_from + delta2d;
                }
                if (point == 0 || point == 2) {
                    en->m_to = en_last.m_to + delta2d;
                }
                if (point == 0 || point == 3) {
                    en->m_center = en_last.m_center + delta2d;
                }
            }
            if (auto en = dynamic_cast<EntityArc3D *>(entity)) {
                auto &en_last = dynamic_cast<const EntityArc3D &>(*last_doc.m_entities.at(entity->m_uuid));
                if (point == 0 || point == 1) {
                    en->m_from = en_last.m_from + delta;
                }
                if (point == 0 || point == 2) {
                    en->m_to = en_last.m_to + delta;
                }
                if (point == 0 || point == 3) {
                    en->m_center = en_last.m_center + delta;
                }
            }
            if (auto en = dynamic_cast<EntityCircle2D *>(entity)) {
                auto &en_last = dynamic_cast<const EntityCircle2D &>(*last_doc.m_entities.at(entity->m_uuid));
                auto &wrkpl = dynamic_cast<const EntityWorkplane &>(*doc.m_entities.at(en->m_wrkpl));

                if (point == 1) {
                    const auto delta2d =
                            wrkpl.project(get_cursor_pos_for_workplane(wrkpl)) - m_inital_pos_wrkpl.at(wrkpl.m_uuid);
                    en->m_center = en_last.m_center + delta2d;
                }
                else if (point == 0) {
                    const auto initial_radius = glm::length(en_last.m_center - m_inital_pos_wrkpl.at(wrkpl.m_uuid));
                    const auto current_radius = glm::length(en->m_center
                                                            - wrkpl.project(m_intf.get_cursor_pos_for_plane(
                                                                    wrkpl.m_origin, wrkpl.get_normal_vector())));


                    en->m_radius = en_last.m_radius + (current_radius - initial_radius);
                }
            }
            if (auto en = dynamic_cast<EntityCircle3D *>(entity)) {
                auto &en_last = dynamic_cast<const EntityCircle3D &>(*last_doc.m_entities.at(entity->m_uuid));
                en->m_center = en_last.m_center + delta;
            }
            if (auto en = dynamic_cast<EntityPoint2D *>(entity)) {
                auto &en_last = dynamic_cast<const EntityPoint2D &>(*last_doc.m_entities.at(entity->m_uuid));
                auto &wrkpl = dynamic_cast<const EntityWorkplane &>(*doc.m_entities.at(en->m_wrkpl));

                if (point == 0) {
                    const auto delta2d =
                            wrkpl.project(get_cursor_pos_for_workplane(wrkpl)) - m_inital_pos_wrkpl.at(wrkpl.m_uuid);
                    en->m_p = en_last.m_p + delta2d;
                }
            }
        }

        doc.set_group_solve_pending(m_first_group);
        m_core.solve_current(m_dragged_list);

        for (auto sr : m_selection) {
            if (sr.type == SelectableRef::Type::CONSTRAINT) {
                auto constraint = doc.m_constraints.at(sr.item).get();
                auto co_wrkpl = dynamic_cast<const IConstraintWorkplane *>(constraint);
                auto co_movable = dynamic_cast<IConstraintMovable *>(constraint);
                if (co_movable) {
                    auto cdelta = delta;
                    glm::dvec2 delta2d;
                    if (co_wrkpl) {
                        const auto wrkpl_uu = co_wrkpl->get_workplane(get_doc());
                        if (wrkpl_uu) {
                            auto &wrkpl = get_entity<EntityWorkplane>(wrkpl_uu);
                            delta2d = wrkpl.project(m_intf.get_cursor_pos_for_plane(wrkpl.m_origin,
                                                                                    wrkpl.get_normal_vector()))
                                      - m_inital_pos_wrkpl.at(wrkpl.m_uuid);
                            cdelta = wrkpl.transform_relative(delta2d);
                        }
                    }
                    auto &co_last = dynamic_cast<const IConstraintMovable &>(*last_doc.m_constraints.at(sr.item));
                    const auto odelta = (co_movable->get_origin(doc) - co_last.get_origin(last_doc));
                    if (co_movable->offset_is_in_workplane())
                        co_movable->set_offset(co_last.get_offset() + glm::dvec3(delta2d, 0) - odelta);
                    else
                        co_movable->set_offset(co_last.get_offset() + cdelta - odelta);
                }
            }
        }


        return ToolResponse();
    }
    else if (args.type == ToolEventType::ACTION) {
        switch (args.action) {
        case InToolActionID::LMB:

            return ToolResponse::commit();
            break;

        case InToolActionID::LMB_RELEASE:
            if (m_is_transient)
                return ToolResponse::commit();
            break;

        case InToolActionID::RMB:
        case InToolActionID::CANCEL:
            return ToolResponse::revert();

        default:;
        }
    }

    return ToolResponse();
}
} // namespace dune3d
