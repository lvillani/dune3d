#pragma once

namespace dune3d {
class Document;
class IGroupGenerate {
public:
    virtual void generate(Document &doc) const = 0;
};
} // namespace dune3d
