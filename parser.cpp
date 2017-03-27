/*

Copyright 2016 Adam Reichold

This file is part of QMediathekView.

QMediathekView is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

QMediathekView is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with QMediathekView.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "parser.h"

#include <uchar.h>

#include <boost/spirit/include/qi.hpp>

namespace QMediathekView
{

namespace
{

std::string parseCodePoint(const unsigned short codePoint)
{
    std::string result(MB_CUR_MAX, '\0');

    static mbstate_t state;
    c16rtomb(&result[0], codePoint, &state);

    return result;
}

// *INDENT-OFF*

template< typename Iterator, typename Skipper >
struct Grammar : boost::spirit::qi::grammar< Iterator, void(), Skipper >
{
    Show show;
    Processor& processor;

    void setChannel(const std::string& channel)
    {
        if (!channel.empty())
        {
            show.channel = QString::fromStdString(channel);
        }
    }

    void setTopic(const std::string& topic)
    {
        if (!topic.empty())
        {
            show.topic = QString::fromStdString(topic);
        }
    }

    void setTitle(const std::string& title)
    {
        show.title = QString::fromStdString(title);
    }

    void setDate(const boost::fusion::vector< int, int, int >& date)
    {
        using boost::fusion::at_c;

        const auto& day = at_c<0>(date);
        const auto& month = at_c<1>(date);
        const auto& year = at_c<2>(date);

        show.date = QDate(year, month, day);
    }

    void resetDate()
    {
        show.date = {};
    }

    void setTime(const boost::fusion::vector< int, int, int >& time)
    {
        using boost::fusion::at_c;

        const auto& hour = at_c<0>(time);
        const auto& minute = at_c<1>(time);
        const auto& second = at_c<2>(time);

        show.time = QTime(hour, minute, second);
    }

    void resetTime()
    {
        show.time = {};
    }

    void setDuration(const boost::fusion::vector< int, int, int >& duration)
    {
        using boost::fusion::at_c;

        const auto& hour = at_c<0>(duration);
        const auto& minute = at_c<1>(duration);
        const auto& second = at_c<2>(duration);

        show.duration = QTime(hour, minute, second);
    }

    void resetDuration()
    {
        show.duration = {};
    }

    void setDescription(const std::string& description)
    {
        show.description = QString::fromStdString(description);
    }

    void setWebsite(const std::string& website)
    {
        show.website = QString::fromStdString(website);
    }

    void setUrl(const std::string& url)
    {
        show.url = QString::fromStdString(url);
    }

    void setUrlSmallExplicit(const boost::fusion::vector< int, std::string >& replacement)
    {
        using boost::fusion::at_c;

        const auto& offset = at_c<0>(replacement);
        const auto& suffix = at_c<1>(replacement);

        show.urlSmallOffset = offset;
        show.urlSmallSuffix = QString::fromStdString(suffix);
    }

    void setUrlSmallImplicit(const std::string& suffix)
    {
        show.urlSmallOffset = show.url.size();
        show.urlSmallSuffix = QString::fromStdString(suffix);
    }

    void resetUrlSmall()
    {
        show.urlSmallOffset = 0;
        show.urlSmallSuffix.clear();
    }

    void setUrlLargeExplicit(const boost::fusion::vector< int, std::string >& replacement)
    {
        using boost::fusion::at_c;

        const auto& offset = at_c<0>(replacement);
        const auto& suffix = at_c<1>(replacement);

        show.urlLargeOffset = offset;
        show.urlLargeSuffix = QString::fromStdString(suffix);
    }

    void setUrlLargeImplicit(const std::string& suffix)
    {
        show.urlLargeOffset = show.url.size();
        show.urlLargeSuffix = QString::fromStdString(suffix);
    }

    void resetUrlLarge()
    {
        show.urlLargeOffset = 0;
        show.urlLargeSuffix.clear();
    }

    void processEntry()
    {
        processor(show);
    }

    template< typename Attributes >
    using Rule = boost::spirit::qi::rule< Iterator, Attributes, Skipper >;

    Rule< void() > start;

    Rule< void() > headerList;
    Rule< void() > entryList;

    Rule< void() > channelTopicTitleItems;
    Rule< void() > dateTimeDurationItems;
    Rule< void() > descriptionItem;
    Rule< void() > websiteItem;
    Rule< void() > urlItem, urlSmallItem, urlLargeItem;

    Rule< void() > ignoredItem;
    Rule< void() > emptyItem;
    Rule< std::string() > textItem;
    Rule< boost::fusion::vector< int, std::string >() > textReplacementItem;
    Rule< boost::fusion::vector< int, int, int >() > dateItem;
    Rule< boost::fusion::vector< int, int, int >() > timeItem;

    boost::spirit::qi::rule< Iterator, std::string() > escapedText;

    Grammar(Processor& processor)
        : Grammar::base_type(start)
        , processor(processor)
    {
        using std::bind;
        using std::placeholders::_1;

        using boost::spirit::qi::attr;
        using boost::spirit::qi::char_;
        using boost::spirit::qi::int_;
        using boost::spirit::qi::eps;
        using boost::spirit::qi::lexeme;
        using boost::spirit::qi::lit;

        using CodePoint = boost::spirit::qi::uint_parser< unsigned short, 16, 4, 4 >;

        escapedText %= *(~char_("\\\"")
                         | (lit("\\\\") >> attr('\\'))
                         | (lit("\\\"") >> attr('"'))
                         | (lit("\\b") >> attr('\b'))
                         | (lit("\\f") >> attr('\f'))
                         | (lit("\\n") >> attr('\n'))
                         | (lit("\\r") >> attr('\r'))
                         | (lit("\\t") >> attr('\t'))
                         | (lit("\\u") >> CodePoint()[parseCodePoint]));

        ignoredItem %= lexeme[eps
                >> lit('"')
                >> *(~char_("\\\"") | (lit('\\') >> char_("\\\"bfnrtu")))
                >> lit('"')];

        emptyItem %= lit("\"\"");

        textItem %= lexeme[eps
                >> lit('"')
                >> escapedText
                >> lit('"')];

        textReplacementItem %= lexeme[eps
                >> lit('"')
                >> int_
                >> lit('|')
                >> escapedText
                >> lit('"')];

        dateItem %= eps
                >> lit('"')
                >> int_ >> lit('.') >> int_ >> lit('.') >> int_
                >> lit('"');

        timeItem %= eps
                >> lit('"')
                >> int_ >> lit(':') >> int_ >> lit(':') >> int_
                >> lit('"');

        channelTopicTitleItems %= eps
                >> textItem[bind(&Grammar::setChannel, this, _1)] >> lit(',')
                >> textItem[bind(&Grammar::setTopic, this, _1)] >> lit(',')
                >> textItem[bind(&Grammar::setTitle, this, _1)];

        dateTimeDurationItems %= eps
                >> (dateItem[bind(&Grammar::setDate, this, _1)] | emptyItem[bind(&Grammar::resetDate, this)]) >> lit(',')
                >> (timeItem[bind(&Grammar::setTime, this, _1)] | emptyItem[bind(&Grammar::resetTime, this)]) >> lit(',')
                >> (timeItem[bind(&Grammar::setDuration, this, _1)] | emptyItem[bind(&Grammar::resetDuration, this)]);

        descriptionItem %= textItem[bind(&Grammar::setDescription, this, _1)];

        websiteItem %= textItem[bind(&Grammar::setWebsite, this, _1)];

        urlItem %= textItem[bind(&Grammar::setUrl, this, _1)];
        urlSmallItem %= textReplacementItem[bind(&Grammar::setUrlSmallExplicit, this, _1)]
                | textItem[bind(&Grammar::setUrlSmallImplicit, this, _1)]
                | emptyItem[bind(&Grammar::resetUrlSmall, this)];
        urlLargeItem %= textReplacementItem[bind(&Grammar::setUrlLargeExplicit, this, _1)]
                | textItem[bind(&Grammar::setUrlLargeImplicit, this, _1)]
                | emptyItem[bind(&Grammar::resetUrlLarge, this)];

        headerList %= lit("\"Filmliste\"")
                >> lit(':')
                >> lit('[')
                >> ignoredItem % lit(',')
                >> lit(']');

        entryList %= lit("\"X\"")
                >> lit(':')
                >> lit('[')
                >> channelTopicTitleItems >> lit(',')
                >> dateTimeDurationItems >> lit(',')
                >> ignoredItem >> lit(',')
                >> descriptionItem >> lit(',')
                >> urlItem >> lit(',')
                >> websiteItem >> lit(',')
                >> ignoredItem >> lit(',')
                >> ignoredItem >> lit(',')
                >> urlSmallItem >> lit(',')
                >> ignoredItem >> lit(',')
                >> urlLargeItem >> lit(',')
                >> ignoredItem % lit(',')
                >> lit(']');

        start %= eps
                >> lit('{')
                >> headerList % lit(',')
                >> lit(',')
                >> entryList[bind(&Grammar::processEntry, this)] % lit(',')
                >> lit('}');
    }

};

// *INDENT-ON*

} // anonymous

bool parse(const QByteArray& data, Processor& processor)
{
    Grammar< QByteArray::const_iterator, boost::spirit::ascii::space_type > grammar(processor);

    return boost::spirit::qi::phrase_parse(data.begin(), data.end(), grammar, boost::spirit::ascii::space);
}

} // QMediathekView
