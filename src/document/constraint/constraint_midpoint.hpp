#pragma once
#include "constraint.hpp"
#include "iconstraint_workplane.hpp"
#include "document/entity/entity_and_point.hpp"

namespace dune3d {

class Entity;

class ConstraintMidpoint : public Constraint, public IConstraintWorkplane {
public:
    explicit ConstraintMidpoint(const UUID &uu);
    explicit ConstraintMidpoint(const UUID &uu, const json &j);
    static constexpr Type s_type = Type::MIDPOINT;
    Type get_type() const override
    {
        return s_type;
    }
    json serialize() const override;
    std::unique_ptr<Constraint> clone() const override;

    UUID m_line;
    EntityAndPoint m_point;
    UUID m_wrkpl;

    const UUID &get_workplane(const Document &doc) const override
    {
        return m_wrkpl;
    }

    std::set<EntityAndPoint> get_referenced_entities_and_points() const override;

    void accept(ConstraintVisitor &visitor) const override;
};

} // namespace dune3d
