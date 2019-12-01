#include "../../plugins/Fourchan.hpp"
#include <json/reader.h>
#include <string.h>
#include "../../include/DataView.hpp"
#include <tidy.h>
#include <tidybuffio.h>

// API documentation: https://github.com/4chan/4chan-API

static const std::string fourchan_url = "https://a.4cdn.org/";
static const std::string fourchan_image_url = "https://i.4cdn.org/";

// Legacy recaptcha command: curl 'https://www.google.com/recaptcha/api/fallback?k=6Ldp2bsSAAAAAAJ5uyx_lx34lJeEpTLVkP5k04qc' -H 'Referer: https://boards.4channel.org/' -H 'Cookie: CONSENT=YES'

/*
Answering recaptcha:
curl 'https://www.google.com/recaptcha/api/fallback?k=6Ldp2bsSAAAAAAJ5uyx_lx34lJeEpTLVkP5k04qc'
-H 'User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0'
-H 'Referer: https://www.google.com/recaptcha/api/fallback?k=6Ldp2bsSAAAAAAJ5uyx_lx34lJeEpTLVkP5k04qc'
-H 'Content-Type: application/x-www-form-urlencoded'
--data 'c=03AOLTBLQ66PjSi9s8S-R1vUS2Jgm-Z_ghEejvvjAaeF3FoR9MiM0zHhCxuertrCo7MAcFUEqcIg4l2WJzVtrJhJVLkncF12OzCaeIvbm46hgDZDZjLD89-LMn1Zs0TP37P-Hd4cuRG8nHuEBXc2ZBD8CVX-6HAs9VBgSmsgQeKF1PWm1tAMBccJhlh4rAOkpjzaEXMMGOe17N0XViwDYZxLGhe4H8IAG2KNB1fb4rz4YKJTPbL30_FvHw7zkdFtojjWiqVW0yCN6N192dhfd9oKz2r9pGRrR6N4AkkX-L0DsBD4yNK3QRsQn3dB1fs3JRZPAh1yqUqTQYhOaqdggyc1EwL8FZHouGRkHTOcCmLQjyv6zuhi6CJbg&response=1&response=4&response=5&response=7'
*/
/*
Response:
<!DOCTYPE HTML>
<html dir="ltr">
   <head>
      <meta http-equiv="content-type" content="text/html; charset=UTF-8">
      <meta http-equiv="X-UA-Compatible" content="IE=edge">
      <title>reCAPTCHA challenge</title>
      <link rel="stylesheet" href="https://www.gstatic.com/recaptcha/releases/0bBqi43w2fj-Lg1N3qzsqHNu/fallback__ltr.css" type="text/css" charset="utf-8">
   </head>
   <body>
      <div class="fbc">
         <div class="fbc-alert"></div>
         <div class="fbc-header">
            <div class="fbc-logo">
               <div class="fbc-logo-img"></div>
               <div class="fbc-logo-text">reCAPTCHA</div>
            </div>
         </div>
         <div class="fbc-success">
            <div class="fbc-message">Copy this code and paste it in the empty box below</div>
            <div class="fbc-verification-token"><textarea dir="ltr" readonly>03AOLTBLTgp-3_4rll3cj-7vPEuXf3I1T371oFiNjfvmu5XtyraKJis7dxiOMNJxIhgn4qHkxpo4SoEwZvQMq3QdK-CPj-Hlu_GbJ74qKw37QeMqOPGciWeFko53F_GLEh2XWjB2b83WTE2ecXXB2MKQAxVGR09VMxlencPojYU3gOTTPc3hhvMz42w0MBxwaisrocB29oL0zpNfWCwdQHjujeLM0C96_rroeprwSM-DI-3FcBJqinLjvJnLXIw47Uve84pn-VOIdR</textarea></div>
            <script nonce="cP5blNrqx/Tso76NX9b4qw">document.getElementsByClassName("fbc-verification-token")[0].firstChild.onclick = function() {this.select();};</script>
            <div class="fbc-message fbc-valid-time">This code is valid for 2 minutes</div>
         </div>
         <div class="fbc-separator"></div>
         <div class="fbc-privacy"><a href="https://www.google.com/intl/en/policies/privacy/" target="_blank">Privacy</a> - <a href="https://www.google.com/intl/en/policies/terms/" target="_blank">Terms</a></div>
      </div>
      <script nonce="cP5blNrqx/Tso76NX9b4qw">document.body.className += " js-enabled";</script>
   </body>
</html>
*/

/* Posting message:
curl 'https://sys.4chan.org/bant/post'
-H 'User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0'
-H 'Referer: https://boards.4chan.org/'
-H 'Content-Type: multipart/form-data; boundary=---------------------------119561554312148213571335532670'
-H 'Origin: https://boards.4chan.org'
-H 'Cookie: __cfduid=d4bd4932e46bc3272fae4ce7a4e2aac511546800687; 4chan_pass=_SsBuZaATt3dIqfVEWlpemhU5XLQ6i9RC'
--data-binary $'-----------------------------119561554312148213571335532670\r\nContent-Disposition: form-data; name="resto"\r\n\r\n8640736\r\n-----------------------------119561554312148213571335532670\r\nContent-Disposition: form-data; name="com"\r\n\r\n>>8640771\r\nShe looks finnish\r\n-----------------------------119561554312148213571335532670\r\nContent-Disposition: form-data; name="mode"\r\n\r\nregist\r\n-----------------------------119561554312148213571335532670\r\nContent-Disposition: form-data; name="pwd"\r\n\r\n_SsBuZaATt3dIqfVEWlpemhU5XLQ6i9RC\r\n-----------------------------119561554312148213571335532670\r\nContent-Disposition: form-data; name="g-recaptcha-response"\r\n\r\n03AOLTBLS5lshp5aPj5pG6xdVMQ0pHuHxAtJoCEYuPLNKYlsRWNCPQegjB9zgL-vwdGMzjcT-L9iW4bnQ5W3TqUWHOVqtsfnx9GipLUL9o2XbC6r9zy-EEiPde7l6J0WcZbr9nh_MGcUpKl6RGaZoYB3WwXaDq74N5hkmEAbqM_CBtbAVVlQyPmemI2HhO2J6K0yFVKBrBingtIZ6-oXBXZ4jC4rT0PeOuVaH_gf_EBjTpb55ueaPmTbeLGkBxD4-wL1qA8F8h0D8c\r\n-----------------------------119561554312148213571335532670--\r\n'
*/
/* Response if banned:
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="robots" content="noarchive">
<meta name="description" content="&quot;/g/ - Technology&quot; is 4chan's imageboard for discussing computer hardware and software, programming, and general technology.">
<meta name="keywords" content="imageboard,worksafe,computer,technology,hardware,software,microsoft,apple,pc,mobile,programming">
<meta name="referrer" content="origin">
<meta name="viewport" content="width=device-width,initial-scale=1">

<link rel="shortcut icon" href="//s.4cdn.org/image/favicon-ws.ico">
<link rel="stylesheet" title="switch" href="//s.4cdn.org/css/yotsubluenew.692.css"><link rel="alternate stylesheet" style="text/css" href="//s.4cdn.org/css/yotsubanew.692.css" title="Yotsuba New"><link rel="alternate stylesheet" style="text/css" href="//s.4cdn.org/css/yotsubluenew.692.css" title="Yotsuba B New"><link rel="alternate stylesheet" style="text/css" href="//s.4cdn.org/css/futabanew.692.css" title="Futaba New"><link rel="alternate stylesheet" style="text/css" href="//s.4cdn.org/css/burichannew.692.css" title="Burichan New"><link rel="alternate stylesheet" style="text/css" href="//s.4cdn.org/css/photon.692.css" title="Photon"><link rel="alternate stylesheet" style="text/css" href="//s.4cdn.org/css/tomorrow.692.css" title="Tomorrow"><link rel="stylesheet" href="//s.4cdn.org/css/yotsubluemobile.692.css"><link rel="stylesheet" href="//s.4cdn.org/js/prettify/prettify.692.css">
<link rel="canonical" href="http://boards.4channel.org/g/">
<link rel="alternate" title="RSS feed" href="/g/index.rss" type="application/rss+xml">
<title>/g/ - Technology - 4chan</title><script type="text/javascript">
var style_group = "ws_style",
cssVersion = 692,
jsVersion = 1042,
comlen = 2000,
maxFilesize = 4194304,
maxLines = 100,
clickable_ids = 1,
cooldowns = {"thread":600,"reply":60,"image":60}
;
var maxWebmFilesize = 3145728;var board_archived = true;var check_for_block = 1;is_error = "true";</script><script type="text/javascript" data-cfasync="false" src="//s.4cdn.org/js/core.min.1042.js"></script>

<noscript><style type="text/css">#postForm { display: table !important; }#g-recaptcha { display: none; }</style></noscript>
</head>
<body class="is_index board_g">
<span id="id_css"></span>

<div id="boardNavDesktop" class="desktop">
<span class="boardList">
[<a href="https://boards.4channel.org/a/" title="Anime & Manga">a</a> / 
<span class="nwsb"><a href="//boards.4chan.org/b/" title="Random">b</a> / </span>
<a href="https://boards.4channel.org/c/" title="Anime/Cute">c</a> / 
<span class="nwsb"><a href="//boards.4chan.org/d/" title="Hentai/Alternative">d</a> / </span>
<span class="nwsb"><a href="//boards.4chan.org/e/" title="Ecchi">e</a> / </span>
<span class="nwsb"><a href="//boards.4chan.org/f/" title="Flash">f</a> / </span>
<a href="https://boards.4channel.org/g/" title="Technology">g</a> / 
<span class="nwsb"><a href="//boards.4chan.org/gif/" title="Adult GIF">gif</a> / </span>
<span class="nwsb"><a href="//boards.4chan.org/h/" title="Hentai">h</a> / </span>
<span class="nwsb"><a href="//boards.4chan.org/hr/" title="High Resolution">hr</a> / </span>
<a href="https://boards.4channel.org/k/" title="Weapons">k</a> / 
<a href="https://boards.4channel.org/m/" title="Mecha">m</a> / 
<a href="https://boards.4channel.org/o/" title="Auto">o</a> / 
<a href="https://boards.4channel.org/p/" title="Photo">p</a> / 
<span class="nwsb"><a href="//boards.4chan.org/r/" title="Adult Requests">r</a> / </span>
<span class="nwsb"><a href="//boards.4chan.org/s/" title="Sexy Beautiful Women">s</a> / </span>
<span class="nwsb"><a href="//boards.4chan.org/t/" title="Torrents">t</a> / </span>
<span class="nwsb"><a href="//boards.4chan.org/u/" title="Yuri">u</a> / </span>
<a href="https://boards.4channel.org/v/" title="Video Games">v</a> / 
<a href="https://boards.4channel.org/vg/" title="Video Game Generals">vg</a> / 
<a href="https://boards.4channel.org/vr/" title="Retro Games">vr</a> / 
<a href="https://boards.4channel.org/w/" title="Anime/Wallpapers">w</a>
<span class="nwsb"> / <a href="//boards.4chan.org/wg/" title="Wallpapers/General">wg</a></span>] 

<span class="nwsb">[<a href="//boards.4chan.org/i/" title="Oekaki">i</a> / 
<a href="https://boards.4channel.org/ic/" title="Artwork/Critique">ic</a>] </span>

[<span class="nwsb"><a href="//boards.4chan.org/r9k/" title="ROBOT9001">r9k</a> / </span>
<span class="nwsb"><a href="//boards.4chan.org/s4s/" title="Shit 4chan Says">s4s</a> / </span>
<a href="https://boards.4channel.org/vip/" title="Very Important Posts">vip</a> / 
<a href="https://boards.4channel.org/qa/" title="Question &amp; Answer">qa</a>] 

[<a href="https://boards.4channel.org/cm/" title="Cute/Male">cm</a> / 
<span class="nwsb"><a href="//boards.4chan.org/hm/" title="Handsome Men">hm</a> / </span>
<a href="https://boards.4channel.org/lgbt/" title="LGBT">lgbt</a>
<span class="nwsb"> / <a href="//boards.4chan.org/y/" title="Yaoi">y</a></span>] 

[<a href="https://boards.4channel.org/3/" title="3DCG">3</a> / 
<span class="nwsb"><a href="//boards.4chan.org/aco/" title="Adult Cartoons">aco</a> / </span>
<a href="https://boards.4channel.org/adv/" title="Advice">adv</a> / 
<a href="https://boards.4channel.org/an/" title="Animals & Nature">an</a> / 
<a href="https://boards.4channel.org/asp/" title="Alternative Sports">asp</a> / 
<span class="nwsb"><a href="//boards.4chan.org/bant/" title="International/Random">bant</a> / </span>
<a href="https://boards.4channel.org/biz/" title="Business & Finance">biz</a> / 
<a href="https://boards.4channel.org/cgl/" title="Cosplay & EGL">cgl</a> / 
<a href="https://boards.4channel.org/ck/" title="Food & Cooking">ck</a> / 
<a href="https://boards.4channel.org/co/" title="Comics & Cartoons">co</a> / 
<a href="https://boards.4channel.org/diy/" title="Do It Yourself">diy</a> / 
<a href="https://boards.4channel.org/fa/" title="Fashion">fa</a> / 
<a href="https://boards.4channel.org/fit/" title="Fitness">fit</a> / 
<a href="https://boards.4channel.org/gd/" title="Graphic Design">gd</a> / 
<span class="nwsb"><a href="//boards.4chan.org/hc/" title="Hardcore">hc</a> / </span>
<a href="https://boards.4channel.org/his/" title="History & Humanities">his</a> / 
<a href="https://boards.4channel.org/int/" title="International">int</a> / 
<a href="https://boards.4channel.org/jp/" title="Otaku Culture">jp</a> / 
<a href="https://boards.4channel.org/lit/" title="Literature">lit</a> / 
<a href="https://boards.4channel.org/mlp/" title="Pony">mlp</a> / 
<a href="https://boards.4channel.org/mu/" title="Music">mu</a> / 
<a href="https://boards.4channel.org/n/" title="Transportation">n</a> / 
<a href="https://boards.4channel.org/news/" title="Current News">news</a> / 
<a href="https://boards.4channel.org/out/" title="Outdoors">out</a> / 
<a href="https://boards.4channel.org/po/" title="Papercraft & Origami">po</a> / 
<span class="nwsb"><a href="//boards.4chan.org/pol/" title="Politically Incorrect">pol</a> / </span>
<a href="https://boards.4channel.org/qst/" title="Quests">qst</a> / 
<a href="https://boards.4channel.org/sci/" title="Science & Math">sci</a> / 
<span class="nwsb"><a href="//boards.4chan.org/soc/" title="Cams & Meetups">soc</a> / </span>
<a href="https://boards.4channel.org/sp/" title="Sports">sp</a> / 
<a href="https://boards.4channel.org/tg/" title="Traditional Games">tg</a> / 
<a href="https://boards.4channel.org/toy/" title="Toys">toy</a> / 
<a href="https://boards.4channel.org/trv/" title="Travel">trv</a> / 
<a href="https://boards.4channel.org/tv/" title="Television & Film">tv</a> / 
<a href="https://boards.4channel.org/vp/" title="Pok&eacute;mon">vp</a> / 
<a href="https://boards.4channel.org/wsg/" title="Worksafe GIF">wsg</a> / 
<a href="https://boards.4channel.org/wsr/" title="Worksafe Requests">wsr</a> / 
<a href="https://boards.4channel.org/x/" title="Paranormal">x</a>] 

</span>

<span id="navtopright">[<a href="javascript:void(0);" id="settingsWindowLink">Settings</a>] 
[<a href="/search" title="Search">Search</a>] 
[<a href="//p.4chan.org/" title="Mobile">Mobile</a>] 
[<a href="//www.4channel.org/" target="_top">Home</a>]</span>
</div>

<div id="boardNavMobile" class="mobile">
    <div class="boardSelect">
        <strong>Board</strong>
        <select id="boardSelectMobile"></select>
    </div>

    <div class="pageJump">
        <a href="#bottom">&#9660;</a>
        <a href="javascript:void(0);" id="settingsWindowLinkMobile">Settings</a>
        <a href="//p.4chan.org/">Mobile</a>
        <a href="//www.4channel.org" target="_top">Home</a>
    </div>

</div>


<div class="boardBanner">
	<div id="bannerCnt" class="title desktop" data-src="189.jpg"></div>
	<div class="boardTitle">/g/ - Technology</div>
	
</div>
<hr class="abovePostForm"><script type="text/javascript">
  setTimeout(function() { window.location = "https://www.4channel.org/banned"; }, 3000);
</script><table style="text-align: center; width: 100%; height: 300px;"><tr valign="middle"><td align="center" style="font-size: x-large; font-weight: bold;"><span id="errmsg" style="color: red;">Error: You are <a href="https://www.4channel.org/banned" target="_blank">banned</a>.</span><br><br>[<a href=//boards.4channel.org/g/>Return</a>]</td></tr></table><br><br><hr size=1><div id="absbot" class="absBotText">

<div class="mobile">
<span id="disable-mobile">[<a href="javascript:disableMobile();">Disable Mobile View / Use Desktop Site</a>]<br><br></span>
<span id="enable-mobile">[<a href="javascript:enableMobile();">Enable Mobile View / Use Mobile Site</a>]<br><br></span>
</div>

<span class="absBotDisclaimer">
All trademarks and copyrights on this page are owned by their respective parties. Images uploaded are the responsibility of the Poster. Comments are owned by the Poster.
</span>
<div id="footer-links"><a href="//www.4channel.org/faq#what4chan" target="_blank">About</a> &bull; <a href="//www.4channel.org/feedback" target="_blank">Feedback</a> &bull; <a href="//www.4channel.org/legal" target="_blank">Legal</a> &bull; <a href="//www.4channel.org/contact" target="_blank">Contact</a></div>
</div>

<div id="bottom"></div>
<script type="text/javascript" src="//s.4cdn.org/js/prettify/prettify.1042.js"></script><script type="text/javascript">prettyPrint();</script><script>var a= document.createElement('script');a.src = 'https://powerad.ai/script.js';a.setAttribute('async','');top.document.querySelector('head').appendChild(a);</script></body></html>
*/
/* Banned page:
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<meta name="keywords" content="imageboard,image board,forum,bbs,anonymous,chan,anime,manga,ecchi,hentai,video games,english,japan">
<meta name="robots" content="noarchive">
<title>4chan - Banned</title><link rel="stylesheet" type="text/css" href="//s.4cdn.org/css/yui.8.css">
<link rel="stylesheet" type="text/css" href="//s.4cdn.org/css/global.61.css">
<link rel="shortcut icon" href="//s.4cdn.org/image/favicon.ico">
<link rel="apple-touch-icon" href="//s.4cdn.org/image/apple-touch-icon-iphone.png">
<link rel="apple-touch-icon" sizes="72x72" href="//s.4cdn.org/image/apple-touch-icon-ipad.png">
<link rel="apple-touch-icon" sizes="114x114" href="//s.4cdn.org/image/apple-touch-icon-iphone-retina.png">
<link rel="apple-touch-icon" sizes="144x144" href="//s.4cdn.org/image/apple-touch-icon-ipad-retina.png">
<link rel="stylesheet" type="text/css" href="//s.4cdn.org/css/banned.14.css">
<script async="" src="//www.google-analytics.com/analytics.js"></script><script type="text/javascript">(function(i,s,o,g,r,a,m){i['GoogleAnalyticsObject']=r;i[r]=i[r]||function(){(i[r].q=i[r].q||[]).push(arguments)},i[r].l=1*new Date();a=s.createElement(o),m=s.getElementsByTagName(o)[0];a.async=1;a.src=g;m.parentNode.insertBefore(a,m)})(window,document,'script','//www.google-analytics.com/analytics.js','ga');
var tid = /(^|\.)4channel.org$/.test(location.host) ? 'UA-166538-5' : 'UA-166538-1';
ga('create', tid, {'sampleRate': 1});
ga('set', 'anonymizeIp', true);
ga('send','pageview')</script>
<script type="text/javascript" src="//s.4cdn.org/js/banned.14.js"></script>
<style type="text/css">#verify-btn { margin-top: 15px; }</style><style type="text/css">#quote-preview {display: block;position: absolute;padding: 3px 6px 6px 3px;margin: 0;text-align: left;border-width: 1px 2px 2px 1px;border-style: solid;}#quote-preview.nws {color: #800000;border-color: #D9BFB7;}#quote-preview.ws {color: #000;border-color: #B7C5D9;}#quote-preview.ws a {color: #34345C;}#quote-preview input {margin: 3px 3px 3px 4px;}.ws.reply {background-color: #D6DAF0;}.nws.reply {background-color: #F0E0D6;}.subject {font-weight: bold;}.ws .subject {color: #0F0C5D;}.nws .subject {color: #CC1105;}.quote {color: #789922;}.quotelink,.deadlink {color: #789922 !important;}.ws .useremail .postertrip,.ws .useremail .name {color: #34345C !important;}.nws .useremail .postertrip,.nws .useremail .name {color: #0000EE !important;}.nameBlock {display: inline-block;}.name {color: #117743;font-weight: bold;}.postertrip {color: #117743;font-weight: normal !important;}.postNum a {text-decoration: none;}.ws .postNum a {color: #000 !important;}.nws .postNum a {color: #800000 !important;}.fileInfo {margin-left: 20px;}.fileThumb {float: left;margin: 3px 20px 5px;}.fileThumb img {border: none;float: left;}s {background-color: #000000 !important;}.capcode {font-weight: bold !important;}.nameBlock.capcodeAdmin span.name, span.capcodeAdmin span.name a, span.capcodeAdmin span.postertrip, span.capcodeAdmin strong.capcode {color: #FF0000 !important;}.nameBlock.capcodeMod span.name, span.capcodeMod span.name a, span.capcodeMod span.postertrip, span.capcodeMod strong.capcode {color: #800080 !important;}.nameBlock.capcodeDeveloper span.name, span.capcodeDeveloper span.name a, span.capcodeDeveloper span.postertrip, span.capcodeDeveloper strong.capcode {color: #0000F0 !important;}.identityIcon {height: 16px;margin-bottom: -3px;width: 16px;}.postMessage {margin: 13px 40px 13px 40px;}.countryFlag {margin-bottom: -1px;padding-top: 1px;}.fileDeletedRes {height: 13px;width: 127px;}span.fileThumb, span.fileThumb img {float: none !important;margin-bottom: 0 !important;margin-top: 0 !important;}</style><style>#captcha-cnt {
  height: auto;
}
:root:not(.js-enabled) #form {
  display: block;
}
#bd > div[style], #bd > div[style] > * {
  height: auto !important;
  margin: 0 !important;
  font-size: 0;
}
</style></head>
<body>
<div id="doc">
<div id="hd">
<div id="logo-fp">
<a href="//www.4chan.org/" title="Home"><img alt="4chan" src="//s.4cdn.org/image/fp/logo-transparent.png" width="300" height="120"></a>
</div>
</div>
<div class="box-outer top-box">
<div class="box-inner">
<div class="boxbar">
<h2>You are <span class="banType">banned</span>! ;_;</h2>
</div>
<div class="boxcontent">
<img src="https://sys.4chan.org/image/error/banned/250/rid.php" alt="Banned" style="float: right; padding-left: 10px; min-height: 150px;" width="250">
<script type="text/javascript">var banjson_0 = {"no":73505519,"now":"11\/09\/19(Sat)16:10:01","name":"Anonymous","com":"<span class=\"deadlink\">&gt;&gt;73505385<\/span><br>People like you are considered right-wing now, according to the left (since the left has gone futher left, they think everybody else has gone further right. There are studies on on political leaning over time that matches this)","id":"","board":"g","ws_board":1}</script>
You have been banned from <b class="board">/g/</b> for posting <a href="#" class="bannedPost_0">&gt;&gt;73505519</a>, a violation of <b>Rule 1</b>:<br><br>
<b class="reason">Off-topic; all images and discussion should pertain to technology and related topics.</b><br><br>
Your ban was filed on <b class="startDate">November 9th, 2019</b> and expires on <b class="endDate">November 10th, 2019 at 22:10 ET</b>, which is 23 hours and 14 minutes from now.
<br><br>
According to our server, your IP is: <b class="bannedIp">YOURIP</b>.	The name you were posting with was <span class="nameBlock"><span class="name">Anonymous</span></span>.<br>
<br>
Because of the short length of your ban, you may not appeal it. Please check back when your ban has expired.
<br class="clear-bug">
</div>
</div>
</div>
<div class="yui-g">
<div class="yui-u first">
</div><div class="yui-u">
</div>
</div>
</div>
<div id="ft"><ul><li class="fill">
</li><li class="first"><a href="/">Home</a></li>
<li><a href="/4channews.php">News</a></li>
<li><a href="http://blog.4chan.org/">Blog</a></li>
<li><a href="/faq">FAQ</a></li>
<li><a href="/rules">Rules</a></li>
<li><a href="/pass">Support 4chan</a></li>
<li><a href="/advertise">Advertise</a></li>
<li><a href="/press">Press</a></li>
<li><a href="/japanese">日本語</a></li>
</ul>
<br class="clear-bug">
<div id="copyright"><a href="/faq#what4chan">About</a> • <a href="/feedback">Feedback</a> • <a href="/legal">Legal</a> • <a href="/contact">Contact</a><br><br><br>
Copyright © 2003-2019 4chan community support LLC. All rights reserved.
</div>
</div>

<div id="modal-bg"></div>


</body>
*/

namespace QuickMedia {
    PluginResult Fourchan::get_front_page(BodyItems &result_items) {
        std::string server_response;
        if(download_to_string(fourchan_url + "boards.json", server_response) != DownloadResult::OK)
            return PluginResult::NET_ERR;

        Json::Value json_root;
        Json::CharReaderBuilder json_builder;
        std::unique_ptr<Json::CharReader> json_reader(json_builder.newCharReader());
        std::string json_errors;
        if(!json_reader->parse(server_response.data(), server_response.data() + server_response.size(), &json_root, &json_errors)) {
            fprintf(stderr, "4chan front page json error: %s\n", json_errors.c_str());
            return PluginResult::ERR;
        }

        const Json::Value &boards = json_root["boards"];
        if(boards.isArray()) {
            for(const Json::Value &board : boards) {
                const Json::Value &board_id = board["board"]; // /g/, /a/, /b/ etc
                const Json::Value &board_title = board["title"];
                const Json::Value &board_description = board["meta_description"];
                if(board_id.isString() && board_title.isString() && board_description.isString()) {
                    std::string board_description_str = board_description.asString();
                    html_unescape_sequences(board_description_str);
                    auto body_item = std::make_unique<BodyItem>("/" + board_id.asString() + "/ " + board_title.asString());
                    body_item->url = board_id.asString();
                    result_items.emplace_back(std::move(body_item));
                }
            }
        }

        return PluginResult::OK;
    }

    SearchResult Fourchan::search(const std::string &url, BodyItems &result_items) {
        return SearchResult::OK;
    }

    SuggestionResult Fourchan::update_search_suggestions(const std::string &text, BodyItems &result_items) {
        return SuggestionResult::OK;
    }

    static bool string_ends_with(const std::string &str, const std::string &ends_with_str) {
        size_t ends_len = ends_with_str.size();
        return ends_len == 0 || (str.size() >= ends_len && memcmp(&str[str.size() - ends_len], ends_with_str.data(), ends_len) == 0);
    }

    struct CommentPiece {
        enum class Type {
            TEXT,
            QUOTE, // >
            QUOTELINK, // >>POSTNO,
            LINE_CONTINUE
        };

        DataView text; // Set when type is TEXT, QUOTE or QUOTELINK
        int64_t quote_postnumber; // Set when type is QUOTELINK
        Type type;
    };

    static TidyAttr get_attribute_by_name(TidyNode node, const char *name) {
        for(TidyAttr attr = tidyAttrFirst(node); attr; attr = tidyAttrNext(attr)) {
            const char *attr_name = tidyAttrName(attr);
            if(attr_name && strcmp(name, attr_name) == 0)
                return attr;
        }
        return nullptr;
    }

    static const char* get_attribute_value(TidyNode node, const char *name) {
        TidyAttr attr = get_attribute_by_name(node, name);
        if(attr)
            return tidyAttrValue(attr);
        return nullptr;
    }

    using CommentPieceCallback = std::function<void(const CommentPiece&)>;
    static void extract_comment_pieces(TidyDoc doc, TidyNode node, CommentPieceCallback callback) {
        for(TidyNode child = tidyGetChild(node); child; child = tidyGetNext(child)) {
            const char *node_name = tidyNodeGetName(child);
            if(node_name && strcmp(node_name, "wbr") == 0) {
                CommentPiece comment_piece;
                comment_piece.type = CommentPiece::Type::LINE_CONTINUE;
                comment_piece.text = { (char*)"", 0 };
                callback(comment_piece);
                continue;
            }
            TidyNodeType node_type = tidyNodeGetType(child);
            if(node_type == TidyNode_Start && node_name) {
                TidyNode text_node = tidyGetChild(child);
                //fprintf(stderr, "Child node name: %s, child text type: %d\n", node_name, tidyNodeGetType(text_node));
                if(tidyNodeGetType(text_node) == TidyNode_Text) {
                    TidyBuffer tidy_buffer;
                    tidyBufInit(&tidy_buffer);
                    if(tidyNodeGetText(doc, text_node, &tidy_buffer)) {
                        CommentPiece comment_piece;
                        comment_piece.type = CommentPiece::Type::TEXT;
                        comment_piece.text = { (char*)tidy_buffer.bp, tidy_buffer.size };
                        if(strcmp(node_name, "span") == 0) {
                            const char *span_class = get_attribute_value(child, "class");
                            //fprintf(stderr, "span class: %s\n", span_class);
                            if(span_class && strcmp(span_class, "quote") == 0)
                                comment_piece.type = CommentPiece::Type::QUOTE;
                        } else if(strcmp(node_name, "a") == 0) {
                            const char *a_class = get_attribute_value(child, "class");
                            const char *a_href = get_attribute_value(child, "href");
                            //fprintf(stderr, "a class: %s, href: %s\n", a_class, a_href);
                            if(a_class && a_href && strcmp(a_class, "quotelink") == 0 && strncmp(a_href, "#p", 2) == 0) {
                                comment_piece.type = CommentPiece::Type::QUOTELINK;
                                comment_piece.quote_postnumber = strtoll(a_href + 2, nullptr, 10);
                            }
                        }
                        callback(comment_piece);
                    }
                    tidyBufFree(&tidy_buffer);
                }
            } else if(node_type == TidyNode_Text) {
                TidyBuffer tidy_buffer;
                tidyBufInit(&tidy_buffer);
                if(tidyNodeGetText(doc, child, &tidy_buffer)) {
                    CommentPiece comment_piece;
                    comment_piece.type = CommentPiece::Type::TEXT;
                    comment_piece.text = { (char*)tidy_buffer.bp, tidy_buffer.size };
                    callback(comment_piece);
                }
                tidyBufFree(&tidy_buffer);
            }
        }
    }

    static void extract_comment_pieces(const char *html_source, size_t size, CommentPieceCallback callback) {
        TidyDoc doc = tidyCreate();
        for(int i = 0; i < N_TIDY_OPTIONS; ++i)
            tidyOptSetBool(doc, (TidyOptionId)i, no);
        if(tidyParseString(doc, html_source) < 0) {
            CommentPiece comment_piece;
            comment_piece.type = CommentPiece::Type::TEXT;
            // Warning: Cast from const char* to char* ...
            comment_piece.text = { (char*)html_source, size };
            callback(comment_piece);
        } else {
            extract_comment_pieces(doc, tidyGetBody(doc), std::move(callback));
        }
        tidyRelease(doc);
    }

    PluginResult Fourchan::get_threads(const std::string &url, BodyItems &result_items) {
        std::string server_response;
        if(download_to_string(fourchan_url + url + "/catalog.json", server_response) != DownloadResult::OK)
            return PluginResult::NET_ERR;

        Json::Value json_root;
        Json::CharReaderBuilder json_builder;
        std::unique_ptr<Json::CharReader> json_reader(json_builder.newCharReader());
        std::string json_errors;
        if(!json_reader->parse(server_response.data(), server_response.data() + server_response.size(), &json_root, &json_errors)) {
            fprintf(stderr, "4chan catalog json error: %s\n", json_errors.c_str());
            return PluginResult::ERR;
        }

        if(json_root.isArray()) {
            for(const Json::Value &page_data : json_root) {
                if(!page_data.isObject())
                    continue;

                const Json::Value &threads = page_data["threads"];
                if(!threads.isArray())
                    continue;

                for(const Json::Value &thread : threads) {
                    if(!thread.isObject())
                        continue;

                    const Json::Value &com = thread["com"];
                    const char *comment_begin = "";
                    const char *comment_end = comment_begin;
                    com.getString(&comment_begin, &comment_end);

                    const Json::Value &thread_num = thread["no"];
                    if(!thread_num.isNumeric())
                        continue;

                    std::string comment_text;
                    extract_comment_pieces(comment_begin, comment_end - comment_begin,
                        [&comment_text](const CommentPiece &cp) {
                            switch(cp.type) {
                                case CommentPiece::Type::TEXT:
                                    comment_text.append(cp.text.data, cp.text.size);
                                    break;
                                case CommentPiece::Type::QUOTE:
                                    comment_text += '>';
                                    comment_text.append(cp.text.data, cp.text.size);
                                    //comment_text += '\n';
                                    break;
                                case CommentPiece::Type::QUOTELINK: {
                                    comment_text.append(cp.text.data, cp.text.size);
                                    break;
                                }
                                case CommentPiece::Type::LINE_CONTINUE: {
                                    if(!comment_text.empty() && comment_text.back() == '\n') {
                                        comment_text.pop_back();
                                    }
                                    break;
                                }
                            }
                        }
                    );
                    html_unescape_sequences(comment_text);
                    // TODO: Do the same when wrapping is implemented
                    int num_lines = 0;
                    for(size_t i = 0; i < comment_text.size(); ++i) {
                        if(comment_text[i] == '\n') {
                            ++num_lines;
                            if(num_lines == 6) {
                                comment_text = comment_text.substr(0, i) + " (...)";
                                break;
                            }
                        }
                    }
                    auto body_item = std::make_unique<BodyItem>(std::move(comment_text));
                    body_item->url = std::to_string(thread_num.asInt64());

                    const Json::Value &ext = thread["ext"];
                    const Json::Value &tim = thread["tim"];
                    if(tim.isNumeric() && ext.isString()) {
                        std::string ext_str = ext.asString();
                        if(ext_str == ".png" || ext_str == ".jpg" || ext_str == ".jpeg") {
                        } else {
                            fprintf(stderr, "TODO: Support file extension: %s\n", ext_str.c_str());
                        }
                        // "s" means small, that's the url 4chan uses for thumbnails.
                        // thumbnails always has .jpg extension even if they are gifs or webm.
                        body_item->thumbnail_url = fourchan_image_url + url + "/" + std::to_string(tim.asInt64()) + "s.jpg";
                    }
                    
                    result_items.emplace_back(std::move(body_item));
                }
            }
        }

        return PluginResult::OK;
    }

    PluginResult Fourchan::get_thread_comments(const std::string &list_url, const std::string &url, BodyItems &result_items) {
        std::string server_response;
        if(download_to_string(fourchan_url + list_url + "/thread/" + url + ".json", server_response) != DownloadResult::OK)
            return PluginResult::NET_ERR;

        Json::Value json_root;
        Json::CharReaderBuilder json_builder;
        std::unique_ptr<Json::CharReader> json_reader(json_builder.newCharReader());
        std::string json_errors;
        if(!json_reader->parse(server_response.data(), server_response.data() + server_response.size(), &json_root, &json_errors)) {
            fprintf(stderr, "4chan thread json error: %s\n", json_errors.c_str());
            return PluginResult::ERR;
        }

        std::unordered_map<int64_t, size_t> comment_by_postno;

        const Json::Value &posts = json_root["posts"];
        if(posts.isArray()) {
            for(const Json::Value &post : posts) {
                if(!post.isObject())
                    continue;

                const Json::Value &post_num = post["no"];
                if(!post_num.isNumeric())
                    continue;
                
                comment_by_postno[post_num.asInt64()] = result_items.size();
                result_items.push_back(std::make_unique<BodyItem>(""));
            }
        }

        size_t body_item_index = 0;
        if(posts.isArray()) {
            for(const Json::Value &post : posts) {
                if(!post.isObject())
                    continue;

                const Json::Value &com = post["com"];
                const char *comment_begin = "";
                const char *comment_end = comment_begin;
                com.getString(&comment_begin, &comment_end);

                const Json::Value &post_num = post["no"];
                if(!post_num.isNumeric())
                    continue;

                std::string comment_text;
                extract_comment_pieces(comment_begin, comment_end - comment_begin,
                    [&comment_text, &comment_by_postno, &result_items, body_item_index](const CommentPiece &cp) {
                        switch(cp.type) {
                            case CommentPiece::Type::TEXT:
                                comment_text.append(cp.text.data, cp.text.size);
                                break;
                            case CommentPiece::Type::QUOTE:
                                comment_text += '>';
                                comment_text.append(cp.text.data, cp.text.size);
                                //comment_text += '\n';
                                break;
                            case CommentPiece::Type::QUOTELINK: {
                                comment_text.append(cp.text.data, cp.text.size);
                                auto it = comment_by_postno.find(cp.quote_postnumber);
                                if(it == comment_by_postno.end()) {
                                    // TODO: Link this quote to a 4chan archive that still has the quoted comment (if available)
                                    comment_text += "(dead)";
                                } else {
                                    result_items[it->second]->replies.push_back(body_item_index);
                                }
                                break;
                            }
                            case CommentPiece::Type::LINE_CONTINUE: {
                                if(!comment_text.empty() && comment_text.back() == '\n') {
                                    comment_text.pop_back();
                                }
                                break;
                            }
                        }
                    }
                );
                html_unescape_sequences(comment_text);
                BodyItem *body_item = result_items[body_item_index].get();
                body_item->set_title(std::move(comment_text));

                const Json::Value &ext = post["ext"];
                const Json::Value &tim = post["tim"];
                if(tim.isNumeric() && ext.isString()) {
                    std::string ext_str = ext.asString();
                    if(ext_str == ".png" || ext_str == ".jpg" || ext_str == ".jpeg") {
                    } else {
                        fprintf(stderr, "TODO: Support file extension: %s\n", ext_str.c_str());
                    }
                    // "s" means small, that's the url 4chan uses for thumbnails.
                    // thumbnails always has .jpg extension even if they are gifs or webm.
                    body_item->thumbnail_url = fourchan_image_url + list_url + "/" + std::to_string(tim.asInt64()) + "s.jpg";
                }
                
                ++body_item_index;
            }
        }

        return PluginResult::OK;
    }
}