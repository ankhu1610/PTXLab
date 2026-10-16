#pragma once

#include <string>

namespace ptxlab {

enum class PrimitiveType {
    I32,
    F32,
    Bool,
    Void
};

struct Type {
    PrimitiveType primitive = PrimitiveType::Void;
    bool isPointer = false;

    [[nodiscard]] constexpr bool isInt() const noexcept {
        return primitive == PrimitiveType::I32 && !isPointer;
    }

    [[nodiscard]] constexpr bool isFloat() const noexcept {
        return primitive == PrimitiveType::F32 && !isPointer;
    }

    [[nodiscard]] constexpr bool isBool() const noexcept {
        return primitive == PrimitiveType::Bool && !isPointer;
    }

    [[nodiscard]] constexpr bool isVoid() const noexcept {
        return primitive == PrimitiveType::Void && !isPointer;
    }

    [[nodiscard]] constexpr bool isPtr() const noexcept {
        return isPointer;
    }

    [[nodiscard]] constexpr bool operator==(const Type& other) const noexcept = default;

    [[nodiscard]] static constexpr Type getI32() noexcept {
        return Type{PrimitiveType::I32, false};
    }

    [[nodiscard]] static constexpr Type getF32() noexcept {
        return Type{PrimitiveType::F32, false};
    }

    [[nodiscard]] static constexpr Type getBool() noexcept {
        return Type{PrimitiveType::Bool, false};
    }

    [[nodiscard]] static constexpr Type getVoid() noexcept {
        return Type{PrimitiveType::Void, false};
    }

    [[nodiscard]] static constexpr Type getI32Ptr() noexcept {
        return Type{PrimitiveType::I32, true};
    }

    [[nodiscard]] static constexpr Type getF32Ptr() noexcept {
        return Type{PrimitiveType::F32, true};
    }

    [[nodiscard]] std::string toString() const {
        std::string s;
        switch (primitive) {
            case PrimitiveType::I32:  s = "i32"; break;
            case PrimitiveType::F32:  s = "f32"; break;
            case PrimitiveType::Bool: s = "bool"; break;
            case PrimitiveType::Void: s = "void"; break;
        }
        if (isPointer) {
            s += "*";
        }
        return s;
    }
};

} // namespace ptxlab
