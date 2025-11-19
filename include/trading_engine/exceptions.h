#pragma once
#include <stdexcept>
#include <string>

class InvalidPriceException : public std::invalid_argument {
public:
    explicit InvalidPriceException(const std::string& what_arg) : std::invalid_argument(what_arg) {}
};

class InvalidQuantityException : public std::invalid_argument {
public:
    explicit InvalidQuantityException(const std::string& what_arg) : std::invalid_argument(what_arg) {}
};

class UnsupportedOrderTypeException : public std::invalid_argument {
public:
    explicit UnsupportedOrderTypeException(const std::string& what_arg) : std::invalid_argument(what_arg) {}
};
