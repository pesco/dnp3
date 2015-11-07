#ifndef PROXYFIXTURE_H
#define PROXYFIXTURE_H

#include <dnp3hammer/plugin.h>
#include <dnp3hammer/dissect.h>
#include <utility>
#include <vector>
#include <string>


// plugin hooks - use these to drive events with the fixture
void hook_link_frame(DissectPlugin *self, const DNP3_Frame *frame, const uint8_t *buf, size_t len);
void hook_transport_reject(DissectPlugin *self);
void hook_transport_segment(DissectPlugin *self, const DNP3_Segment *segment);
void hook_transport_payload(DissectPlugin *self, const uint8_t *s, size_t n);
void hook_app_reject(DissectPlugin *self);
void hook_app_error(DissectPlugin *self, DNP3_ParseError e);
void hook_app_fragment(DissectPlugin *self, const DNP3_Fragment *fragment, const uint8_t *buf, size_t len);

// corresponding event enums
enum class Event
{
    LINK_FRAME,
    TRANS_REJECT,
    TRANS_SEGMENT,
    TRANS_PAYLOAD,
    APP_REJECT,
    APP_ERROR,
    APP_FRAG
};


typedef std::pair<const uint8_t*, size_t> slice_t;

class PluginFixture
{
    public:
        PluginFixture();
        ~PluginFixture();

        bool Parse(const std::string& hex);

        bool CheckEvents(std::initializer_list<Event> expected) const;

        // the output that gets written by the plugin
        std::vector<slice_t> writes;

        std::vector<Event> events;

    private:

        static void QueueOutput(void *env, const uint8_t *buf, size_t n);

        Plugin* m_plugin;

};

#endif //PROXYFIXTURE_H
