#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "ptxlab/kc/type.h"
#include "ptxlab/support/source_location.h"

namespace ptxlab {

struct Symbol {
    std::string name;
    Type type;
    SourceLocation declLoc;
    bool isParam = false;
};

class Scope {
public:
    explicit Scope(Scope* parent = nullptr) : parent_(parent) {}

    bool insert(Symbol sym) {
        auto [it, inserted] = symbols_.emplace(sym.name, std::move(sym));
        return inserted;
    }

    [[nodiscard]] const Symbol* lookup(std::string_view name) const {
        auto it = symbols_.find(std::string(name));
        if (it != symbols_.end()) {
            return &it->second;
        }
        if (parent_) {
            return parent_->lookup(name);
        }
        return nullptr;
    }

    [[nodiscard]] const Symbol* lookupCurrent(std::string_view name) const {
        auto it = symbols_.find(std::string(name));
        if (it != symbols_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    [[nodiscard]] Scope* parent() const noexcept { return parent_; }

private:
    Scope* parent_ = nullptr;
    std::unordered_map<std::string, Symbol> symbols_;
};

class SymbolTable {
public:
    SymbolTable() {
        // Start with root global scope
        enterScope();
    }

    void enterScope() {
        auto scope = std::make_unique<Scope>(currentScope_);
        currentScope_ = scope.get();
        allScopes_.push_back(std::move(scope));
    }

    void exitScope() {
        if (currentScope_ && currentScope_->parent()) {
            currentScope_ = currentScope_->parent();
        }
    }

    bool insert(Symbol sym) {
        if (!currentScope_) {
            return false;
        }
        return currentScope_->insert(std::move(sym));
    }

    [[nodiscard]] const Symbol* lookup(std::string_view name) const {
        if (!currentScope_) {
            return nullptr;
        }
        return currentScope_->lookup(name);
    }

    [[nodiscard]] const Symbol* lookupCurrent(std::string_view name) const {
        if (!currentScope_) {
            return nullptr;
        }
        return currentScope_->lookupCurrent(name);
    }

    void clear() {
        allScopes_.clear();
        currentScope_ = nullptr;
        enterScope();
    }

private:
    std::vector<std::unique_ptr<Scope>> allScopes_;
    Scope* currentScope_ = nullptr;
};

} // namespace ptxlab
