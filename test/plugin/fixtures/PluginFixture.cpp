
#include "PluginFixture.h"

#include <assert.h>
#include <cstring>
#include <cstdint>

#include "HexData.h"


void cb_link_frame(void *env, const DNP3_Frame *frame, const uint8_t *buf, size_t len)
{
    static_cast<PluginFixture*>(env)->events.push_back(Event::LINK_FRAME);
}

void cb_transport_segment(void *env, const DNP3_Segment *segment)
{
    static_cast<PluginFixture*>(env)->events.push_back(Event::TRANS_SEGMENT);
}

void cb_transport_payload(void *env, const uint8_t *s, size_t n)
{
    static_cast<PluginFixture*>(env)->events.push_back(Event::TRANS_PAYLOAD);
}

void cb_app_invalid(void *env, DNP3_ParseError e)
{
    static_cast<PluginFixture*>(env)->events.push_back(Event::APP_INVALID);
}

void cb_app_fragment(void *env, const DNP3_Fragment *fragment, const uint8_t *buf, size_t len)
{
    static_cast<PluginFixture*>(env)->events.push_back(Event::APP_FRAG);
}

PluginFixture::PluginFixture()
{
    DNP3_Callbacks callbacks;

    callbacks.link_frame = cb_link_frame;
    callbacks.transport_segment = cb_transport_segment;
    callbacks.transport_payload = cb_transport_payload;
    callbacks.app_invalid = cb_app_invalid;
    callbacks.app_fragment = cb_app_fragment;

    m_plugin = dnp3_dissector(callbacks, this);
    assert(m_plugin);
}

bool PluginFixture::Parse(const std::string& hex)
{
    HexData data(hex);

    assert(m_plugin->bufsize >= data.Size());
    memcpy(m_plugin->buf, data.Buffer(), data.Size());

    return m_plugin->feed(m_plugin, data.Size()) == 0;
}

bool PluginFixture::CheckEvents(std::initializer_list<Event> expected) const
{
    if(expected.size() != events.size())
    {
        return false;
    }

    size_t i = 0;

    for(auto& event: expected)
    {
        if(event != events[i])
        {
            return false;
        }

        ++i;
    }

    return true;
}

PluginFixture::~PluginFixture()
{
    assert(m_plugin->finish(m_plugin) == 0);
}

