
#include "PluginFixture.h"

#include <assert.h>
#include <cstring>
#include <dnp3hammer/dissect.h>

#include "HexData.h"


void hook_link_frame(DissectPlugin *self, const DNP3_Frame *frame, const uint8_t *buf, size_t len)
{
    static_cast<PluginFixture*>(self->env)->events.push_back(Event::LINK_FRAME);
}

void hook_transport_reject(DissectPlugin *self)
{
    static_cast<PluginFixture*>(self->env)->events.push_back(Event::TRANS_REJECT);
}

void hook_transport_segment(DissectPlugin *self, const DNP3_Segment *segment)
{
    static_cast<PluginFixture*>(self->env)->events.push_back(Event::TRANS_SEGMENT);
}

void hook_transport_payload(DissectPlugin *self, const uint8_t *s, size_t n)
{
    static_cast<PluginFixture*>(self->env)->events.push_back(Event::TRANS_PAYLOAD);
}

void hook_app_reject(DissectPlugin *self)
{
    static_cast<PluginFixture*>(self->env)->events.push_back(Event::APP_REJECT);
}

void hook_app_error(DissectPlugin *self, DNP3_ParseError e)
{
    static_cast<PluginFixture*>(self->env)->events.push_back(Event::APP_ERROR);
}

void hook_app_fragment(DissectPlugin *self, const DNP3_Fragment *fragment, const uint8_t *buf, size_t len)
{
    static_cast<PluginFixture*>(self->env)->events.push_back(Event::APP_FRAG);
}

PluginFixture::PluginFixture() :
        m_plugin(dnp3_dissect(&QueueOutput, this))
{
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

void PluginFixture::QueueOutput(void *env, const uint8_t *buf, size_t n)
{
    auto fixture = static_cast<PluginFixture *>(env);
    fixture->writes.push_back(slice_t(buf, n));
}

PluginFixture::~PluginFixture()
{
    assert(m_plugin->finish(m_plugin) == 0);
}

