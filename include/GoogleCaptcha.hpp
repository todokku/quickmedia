#pragma once

#include <string>
#include <optional>
#include <functional>
#include <future>
#include <array>

namespace QuickMedia {
    struct GoogleCaptchaChallengeInfo
    {
        std::string id;
        std::string payload_url;
        std::string description;
    };

    using RequestChallengeResponse = std::function<void(std::optional<GoogleCaptchaChallengeInfo>)>;
    std::future<bool> google_captcha_request_challenge(const std::string &api_key, const std::string &referer, RequestChallengeResponse challenge_response_callback, bool use_tor = false);
    using PostSolutionResponse = std::function<void(std::optional<std::string>, std::optional<GoogleCaptchaChallengeInfo>)>;
    std::future<bool> google_captcha_post_solution(const std::string &api_key, const std::string &captcha_id, std::array<bool, 9> selected_images, PostSolutionResponse solution_response_callback, bool use_tor);
}