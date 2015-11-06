
#include "PluginFixture.h"

#include <assert.h>
#include <cstring>

#include "HexData.h"

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

void PluginFixture::QueueOutput(void *env, const uint8_t *buf, size_t n)
{
    auto fixture = static_cast<PluginFixture *>(env);
    fixture->writes.push_back(slice_t(buf, n));
}

PluginFixture::~PluginFixture()
{
    assert(m_plugin->finish(m_plugin) == 0);
}

