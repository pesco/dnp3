
#include "PluginFixture.h"

#include <assert.h>

PluginFixture::PluginFixture() :
        m_plugin(dnp3_dissect(&QueueOutput, this))
{
    assert(m_plugin);
}

void PluginFixture::QueueOutput(void *env, const uint8_t *buf, size_t n)
{
    auto fixture = static_cast<PluginFixture *>(env);
    fixture->writes.push_back(slice_t(buf, n));
}

PluginFixture::~PluginFixture()
{
    m_plugin->finish(m_plugin);
}

