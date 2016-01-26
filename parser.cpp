#include "parser.h"

#include <boost/spirit/include/qi.hpp>

namespace
{

using namespace Mediathek;

// *INDENT-OFF*

template< typename Iterator, typename Skipper >
struct Grammar : boost::spirit::qi::grammar< Iterator, void(), Skipper >
{
    Show show;
    const Inserter& inserter;

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

    void setDate(const boost::fusion::vector< int, int, int>& date)
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

    void setTime(const boost::fusion::vector< int, int, int>& time)
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

    void setDuration(const boost::fusion::vector< int, int, int>& duration)
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

    void setUrlSmall(const boost::fusion::vector< int, std::string >& replacement)
    {
        using boost::fusion::at_c;

        const auto& offset = at_c<0>(replacement);
        const auto& text = at_c<1>(replacement);

        show.urlSmall = show.url.left(offset) + QString::fromStdString(text);
    }

    void resetUrlSmall()
    {
        show.urlSmall.clear();
    }

    void setUrlLarge(const boost::fusion::vector< int, std::string >& replacement)
    {
        using boost::fusion::at_c;

        const auto& offset = at_c<0>(replacement);
        const auto& text = at_c<1>(replacement);

        show.urlLarge = show.url.left(offset) + QString::fromStdString(text);
    }

    void resetUrlLarge()
    {
        show.urlLarge.clear();
    }

    void insertEntry()
    {
        inserter(show);
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

    Rule< void() > emptyItem;
    Rule< std::string() > textItem;
    Rule< boost::fusion::vector< int, std::string >() > textReplacementItem;
    Rule< boost::fusion::vector< int, int, int >() > dateItem;
    Rule< boost::fusion::vector< int, int, int >() > timeItem;

    boost::spirit::qi::rule< Iterator, std::string() > escapedText;

    Grammar(const Inserter& inserter)
        : Grammar::base_type(start)
        , inserter(inserter)
    {
        using std::bind;
        using std::placeholders::_1;

        using boost::spirit::qi::attr;
        using boost::spirit::qi::char_;
        using boost::spirit::qi::int_;
        using boost::spirit::qi::eps;
        using boost::spirit::qi::lexeme;
        using boost::spirit::qi::lit;

        escapedText %= *(~char_("\\\"")
                         | (lit("\\\\") >> attr('\\'))
                         | (lit("\\\"") >> attr('"'))
                         | (lit("\\b") >> attr('\b'))
                         | (lit("\\f") >> attr('\f'))
                         | (lit("\\n") >> attr('\n'))
                         | (lit("\\r") >> attr('\r'))
                         | (lit("\\t") >> attr('\t')));

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
        urlSmallItem %= textReplacementItem[bind(&Grammar::setUrlSmall, this, _1)] | emptyItem[bind(&Grammar::resetUrlSmall, this)];
        urlLargeItem %= textReplacementItem[bind(&Grammar::setUrlLarge, this, _1)] | emptyItem[bind(&Grammar::resetUrlLarge, this)];

        headerList %= lit("\"Filmliste\"")
                >> lit(':')
                >> lit('[')
                >> textItem % lit(',')
                >> lit(']');

        entryList %= lit("\"X\"")
                >> lit(':')
                >> lit('[')
                >> channelTopicTitleItems >> lit(',')
                >> dateTimeDurationItems >> lit(',')
                >> textItem >> lit(',')
                >> descriptionItem >> lit(',')
                >> urlItem >> lit(',')
                >> websiteItem >> lit(',')
                >> textItem >> lit(',')
                >> textItem >> lit(',')
                >> urlSmallItem >> lit(',')
                >> textItem >> lit(',')
                >> urlLargeItem >> lit(',')
                >> textItem % lit(',')
                >> lit(']');

        start %= eps
                >> lit('{')
                >> headerList % lit(',')
                >> lit(',')
                >> entryList[bind(&Grammar::insertEntry, this)] % lit(',')
                >> lit('}');
    }

};

// *INDENT-ON*

} // anonymous

namespace Mediathek
{

bool parse(const QByteArray& data, const Inserter& inserter)
{
    Grammar< QByteArray::const_iterator, boost::spirit::qi::space_type > grammar(inserter);

    return boost::spirit::qi::phrase_parse(data.begin(), data.end(), grammar, boost::spirit::qi::space);
}

} // Mediathek
