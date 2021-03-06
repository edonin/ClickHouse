#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <Core/Block.h>
#include <ext/singleton.h>
#include <Storages/MergeTree/MergeTreeDataPartChecksum.h>
#include <Storages/SelectQueryInfo.h>
#include <Storages/MergeTree/MarkRange.h>
#include <Interpreters/ExpressionActions.h>
#include <Parsers/ASTIndexDeclaration.h>

constexpr auto INDEX_FILE_PREFIX = "skp_idx_";

namespace DB
{

class MergeTreeData;
class MergeTreeIndex;

using MergeTreeIndexPtr = std::shared_ptr<const MergeTreeIndex>;
using MutableMergeTreeIndexPtr = std::shared_ptr<MergeTreeIndex>;


struct MergeTreeIndexGranule
{
    virtual ~MergeTreeIndexGranule() = default;

    virtual void serializeBinary(WriteBuffer & ostr) const = 0;
    virtual void deserializeBinary(ReadBuffer & istr) = 0;

    virtual String toString() const = 0;
    virtual bool empty() const = 0;

    virtual void update(const Block & block, size_t * pos, size_t limit) = 0;
};


using MergeTreeIndexGranulePtr = std::shared_ptr<MergeTreeIndexGranule>;
using MergeTreeIndexGranules = std::vector<MergeTreeIndexGranulePtr>;

/// Condition on the index.
class IndexCondition
{
public:
    virtual ~IndexCondition() = default;
    /// Checks if this index is useful for query.
    virtual bool alwaysUnknownOrTrue() const = 0;

    virtual bool mayBeTrueOnGranule(MergeTreeIndexGranulePtr granule) const = 0;
};

using IndexConditionPtr = std::shared_ptr<IndexCondition>;


/// Structure for storing basic index info like columns, expression, arguments, ...
class MergeTreeIndex
{
public:
    MergeTreeIndex(
        String name,
        ExpressionActionsPtr expr,
        const Names & columns,
        const DataTypes & data_types,
        const Block & header,
        size_t granularity)
        : name(name)
        , expr(expr)
        , columns(columns)
        , data_types(data_types)
        , header(header)
        , granularity(granularity) {}

    virtual ~MergeTreeIndex() = default;

    /// gets filename without extension
    String getFileName() const { return INDEX_FILE_PREFIX + name; }

    virtual MergeTreeIndexGranulePtr createIndexGranule() const = 0;

    virtual IndexConditionPtr createIndexCondition(
            const SelectQueryInfo & query_info, const Context & context) const = 0;

    String name;
    ExpressionActionsPtr expr;
    Names columns;
    DataTypes data_types;
    Block header;
    size_t granularity;
};


using MergeTreeIndices = std::vector<MutableMergeTreeIndexPtr>;


class MergeTreeIndexFactory : public ext::singleton<MergeTreeIndexFactory>
{
    friend class ext::singleton<MergeTreeIndexFactory>;

public:
    using Creator = std::function<
            std::unique_ptr<MergeTreeIndex>(
                    const NamesAndTypesList & columns,
                    std::shared_ptr<ASTIndexDeclaration> node,
                    const Context & context)>;

    std::unique_ptr<MergeTreeIndex> get(
        const NamesAndTypesList & columns,
        std::shared_ptr<ASTIndexDeclaration> node,
        const Context & context) const;

    void registerIndex(const std::string & name, Creator creator);

    const auto & getAllIndexes() const { return indexes; }

protected:
    MergeTreeIndexFactory() = default;

private:
    using Indexes = std::unordered_map<std::string, Creator>;
    Indexes indexes;
};

}
