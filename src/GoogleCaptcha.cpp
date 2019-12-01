#include "../include/GoogleCaptcha.hpp"
#include "../include/StringUtils.hpp"
#include "../include/DownloadUtils.hpp"

namespace QuickMedia {
    static bool google_captcha_response_extract_id(const std::string& html, std::string& result)
    {
        size_t value_index = html.find("value=\"");
        if(value_index == std::string::npos)
            return false;

        size_t value_begin = value_index + 7;
        size_t value_end = html.find('"', value_begin);
        if(value_end == std::string::npos)
            return false;

        size_t value_length = value_end - value_begin;
        // The id is also only valid if it's in base64, but it might be overkill to verify if it's base64 here
        if(value_length < 300)
            return false;

        result = html.substr(value_begin, value_length);
        return true;
    }

    static bool google_captcha_response_extract_goal_description(const std::string& html, std::string& result)
    {
        size_t goal_description_begin = html.find("rc-imageselect-desc-no-canonical");
        if(goal_description_begin == std::string::npos) {
            goal_description_begin = html.find("rc-imageselect-desc");
            if(goal_description_begin == std::string::npos)
                return false;
        }

        goal_description_begin = html.find('>', goal_description_begin);
        if(goal_description_begin == std::string::npos)
            return false;

        goal_description_begin += 1;

        size_t goal_description_end = html.find("</div", goal_description_begin);
        if(goal_description_end == std::string::npos)
            return false;

        // The goal description with google captcha right now is "Select all images with <strong>subject</strong>".
        // TODO: Should the subject be extracted, so bold styling can be applied to it?
        result = html.substr(goal_description_begin, goal_description_end - goal_description_begin);
        string_replace_all(result, "<strong>", "");
        string_replace_all(result, "</strong>", "");
        return true;
    }

    static std::optional<GoogleCaptchaChallengeInfo> google_captcha_parse_request_challenge_response(const std::string& api_key, const std::string& html_source)
    {
        GoogleCaptchaChallengeInfo result;
        if(!google_captcha_response_extract_id(html_source, result.id))
            return std::nullopt;
        result.payload_url = "https://www.google.com/recaptcha/api2/payload?c=" + result.id + "&k=" + api_key;
        if(!google_captcha_response_extract_goal_description(html_source, result.description))
            return std::nullopt;
        return result;
    }

    // Note: This assumes strings (quoted data) in html tags dont contain '<' or '>'
    static std::string strip_html_tags(const std::string& text)
    {
        std::string result;
        size_t index = 0;
        while(true)
        {
            size_t tag_start_index = text.find('<', index);
            if(tag_start_index == std::string::npos)
            {
                result.append(text.begin() + index, text.end());
                break;
            }

            result.append(text.begin() + index, text.begin() + tag_start_index);
            
            size_t tag_end_index = text.find('>', tag_start_index + 1);
            if(tag_end_index == std::string::npos)
                break;
            index = tag_end_index + 1;
        }
        return result;
    }

    static std::optional<std::string> google_captcha_parse_submit_solution_response(const std::string& html_source)
    {
        size_t start_index = html_source.find("\"fbc-verification-token\">");
        if(start_index == std::string::npos)
            return std::nullopt;

        start_index += 25;
        size_t end_index = html_source.find("</", start_index);
        if(end_index == std::string::npos)
            return std::nullopt;

        return strip_html_tags(html_source.substr(start_index, end_index - start_index));
    }

    std::future<bool> google_captcha_request_challenge(const std::string &api_key, const std::string &referer, RequestChallengeResponse challenge_response_callback, bool use_tor) {
        return std::async(std::launch::async, [challenge_response_callback, api_key, referer, use_tor]() {
            std::string captcha_url = "https://www.google.com/recaptcha/api/fallback?k=" + api_key;
            std::string response;
            std::vector<CommandArg> additional_args = {
                CommandArg{"-H", "Referer: " + referer}
            };
            DownloadResult download_result = download_to_string(captcha_url, response, additional_args, use_tor);
            if(download_result == DownloadResult::OK) {
                //fprintf(stderr, "Failed  to get captcha, response: %s\n", response.c_str());
                challenge_response_callback(google_captcha_parse_request_challenge_response(api_key, response));
                return true;
            } else {
                fprintf(stderr, "Failed  to get captcha, response: %s\n", response.c_str());
                challenge_response_callback(std::nullopt);
                return false;
            }
        });
    }

    static std::string build_post_solution_response(std::array<bool, 9> selected_images) {
        std::string result;
        for(size_t i = 0; i < selected_images.size(); ++i) {
            if(selected_images[i]) {
                if(!result.empty())
                    result += "&";
                result += "response=" + std::to_string(i);
            }
        }
        return result;
    }

    std::future<bool> google_captcha_post_solution(const std::string &api_key, const std::string &captcha_id, std::array<bool, 9> selected_images, PostSolutionResponse solution_response_callback, bool use_tor) {
        return std::async(std::launch::async, [solution_response_callback, api_key, captcha_id, selected_images, use_tor]() {
            std::string captcha_url = "https://www.google.com/recaptcha/api/fallback?k=" + api_key;
            std::string response;
            std::vector<CommandArg> additional_args = {
                CommandArg{"-H", "Referer: " + captcha_url},
                CommandArg{"--data", "c=" + captcha_id + "&" + build_post_solution_response(selected_images)}
            };
            DownloadResult post_result = download_to_string(captcha_url, response, additional_args, use_tor);
            if(post_result == DownloadResult::OK) {
                //fprintf(stderr, "Failed  to post captcha solution, response: %s\n", response.c_str());
                std::optional<std::string> captcha_post_id = google_captcha_parse_submit_solution_response(response);
                if(captcha_post_id) {
                    solution_response_callback(std::move(captcha_post_id), std::nullopt);
                } else {
                    std::optional<GoogleCaptchaChallengeInfo> challenge_info = google_captcha_parse_request_challenge_response(api_key, response);
                    solution_response_callback(std::nullopt, std::move(challenge_info));
                }
                return true;
            } else {
                fprintf(stderr, "Failed  to post captcha solution, response: %s\n", response.c_str());
                solution_response_callback(std::nullopt, std::nullopt);
                return false;
            }
        });
    }
}