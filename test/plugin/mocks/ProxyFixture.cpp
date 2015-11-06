
#include "ProxyFixture.h"

#include <assert.h>

ProxyFixture::ProxyFixture() :
        m_plugin(dnp3_dissect(&QueueOutput, this))
{
    assert(m_plugin);
}

void ProxyFixture::QueueOutput(void *env, const uint8_t *buf, size_t n)
{
    auto fixture = static_cast<ProxyFixture*>(env);
    fixture->writes.push_back(slice_t(buf, n));
}

ProxyFixture::~ProxyFixture()
{
    m_plugin->finish(m_plugin);
}

