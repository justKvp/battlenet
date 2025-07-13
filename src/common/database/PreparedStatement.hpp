#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <iostream>

class PreparedStatement {
public:
    explicit PreparedStatement(const std::string& name) : name_(name) {}

    const std::string& name() const { return name_; }

    const std::vector<std::optional<std::string>>& params() const { return params_; }

    // ======== SET PARAMS ========

    template<typename T>
    void set_param(size_t index, T value) {
        ensure_size(index);
        params_[index] = std::to_string(value);
    }

    void set_param(size_t index, const std::string& value) {
        ensure_size(index);
        params_[index] = value;
    }

    void set_param(size_t index, const char* value) {
        ensure_size(index);
        params_[index] = std::string(value);
    }

    void set_param(size_t index, std::string_view value) {
        ensure_size(index);
        params_[index] = std::string(value);
    }

    void set_null(size_t index) {
        ensure_size(index);
        params_[index].reset();
    }

    // ======== UTILITY ========

    void clear() {
        params_.clear();
    }

    void debug_print() const {
        std::cout << "[PreparedStatement] Name: " << name_ << "\n";
        for (size_t i = 0; i < params_.size(); ++i) {
            if (params_[i].has_value()) {
                std::cout << "  Param[" << i << "] = " << params_[i].value() << "\n";
            } else {
                std::cout << "  Param[" << i << "] = NULL\n";
            }
        }
    }

private:
    void ensure_size(size_t index) {
        if (index >= params_.size()) {
            params_.resize(index + 1);
        }
    }

    std::string name_;
    std::vector<std::optional<std::string>> params_;
};
