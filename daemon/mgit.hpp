// Created by Unium on 12.02.26

#pragma once

#include <string>

namespace mgit {
bool clone(const std::string &url, const std::string &dp,
           const std::string &branch);

bool pull(const std::string &rp);

bool hUpdates(const std::string &rp, const std::string &branch);

std::string cCommit(const std::string &rp);

int rIn(const std::string &dir, const std::string &args,
        std::string *o = nullptr);
} // namespace mgit
