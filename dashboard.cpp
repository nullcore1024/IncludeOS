// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2015-2016 Oslo and Akershus University College of Applied Sciences
// and Alfred Bratterud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dashboard.hpp"
#include <statman>
#include <profile>

using namespace server;
using namespace rapidjson;

Dashboard::Dashboard()
: router_(), buffer_(0, 4096), writer_(buffer_),
stack_samples(12)
{
  setup_routes();
  StackSampler::begin();
}

void Dashboard::setup_routes() {
  router_.on_get("/",
    RouteCallback::from<Dashboard,&Dashboard::serve_all>(this));

  router_.on_get("/memmap",
    RouteCallback::from<Dashboard,&Dashboard::serve_memmap>(this));

  router_.on_get("/statman",
    RouteCallback::from<Dashboard,&Dashboard::serve_statman>(this));

  router_.on_get("/stack_sampler",
    RouteCallback::from<Dashboard,&Dashboard::serve_stack_sampler>(this));
}

void Dashboard::serve_all(Request_ptr, Response_ptr res) {
  writer_.StartObject();

  writer_.Key("memmap");
  serialize_memmap(writer_);

  writer_.Key("statman");
  serialize_statman(writer_);

  writer_.Key("stack_sampler");
  serialize_stack_sampler(writer_);

  writer_.Key("status");
  serialize_status(writer_);

  writer_.EndObject();

  send_buffer(res);
}

void Dashboard::serve_memmap(Request_ptr, Response_ptr res) {
  serialize_memmap(writer_);
  send_buffer(res);
}

void Dashboard::serve_statman(Request_ptr, Response_ptr res) {
  serialize_statman(writer_);
  send_buffer(res);
}

void Dashboard::serve_stack_sampler(Request_ptr, Response_ptr res) {
  serialize_stack_sampler(writer_);
  send_buffer(res);
}

void Dashboard::serialize_memmap(Writer& writer) const {
  writer.StartArray();
  for (auto i : OS::memory_map())
  {
    auto& entry = i.second;
    writer.StartObject();

    writer.Key("name");
    writer.String(entry.name());

    writer.Key("addr_start");
    writer.Uint(entry.addr_start());

    writer.Key("addr_end");
    writer.Uint(entry.addr_end());

    writer.Key("in_use");
    writer.Uint(entry.in_use());

    writer.Key("description");
    writer.String(entry.description());

    writer.EndObject();
  }
  writer.EndArray();
}

void Dashboard::serialize_statman(Writer& writer) const {
  Statman& statman = Statman::get();
  writer.StartArray();
  for(auto it = statman.begin(); it != statman.last_used(); ++it) {
    auto& stat = *it;
    writer.StartObject();

    writer.Key("name");
    writer.String(stat.name());

    writer.Key("value");
    std::string type = "";

    switch(stat.type()) {
      case Stat::UINT64:  writer.Uint64(stat.get_uint64());
                          type = "UINT64";
                          break;
      case Stat::UINT32:  writer.Uint(stat.get_uint32());
                          type = "UINT32";
                          break;
      case Stat::FLOAT:   writer.Double(stat.get_float());
                          type = "FLOAT";
                          break;
    }

    writer.Key("type");
    writer.String(type);

    writer.Key("index");
    writer.Int(stat.index());

    writer.EndObject();
  }

  writer.EndArray();
}


void Dashboard::serialize_stack_sampler(Writer& writer) const {
  auto samples = StackSampler::results(stack_samples);
  int total = StackSampler::samples_total();

  writer.StartArray();
  for (auto& sa : samples)
  {
    writer.StartObject();

    writer.Key("address");
    writer.Uint((uintptr_t)sa.addr);

    writer.Key("name");
    writer.String(sa.name);

    writer.Key("total");
    writer.Uint(sa.samp);

    // percentage of total samples
    float perc = sa.samp / (float)total * 100.0f;

    writer.Key("percent");
    writer.Double(perc);

    writer.EndObject();
  }
  writer.EndArray();
}

void Dashboard::serialize_status(Writer& writer) const {
  writer.StartObject();

  writer.Key("version");
  writer.String(OS::version());

  writer.Key("service");
  writer.String(Service::name());

  writer.Key("uptime");
  writer.Int64(OS::uptime());

  writer.Key("heap_usage");
  writer.Uint64(OS::heap_usage());

  writer.Key("cpu_freq");
  writer.Double(OS::cpu_freq().count());

  writer.Key("current_time");
  long hest = RTC::now();
  struct tm* tt =
    gmtime (&hest);
  char datebuf[32];
  strftime(datebuf, sizeof datebuf, "%FT%TZ", tt);
  writer.String(datebuf);

  writer.EndObject();

}

void Dashboard::send_buffer(server::Response_ptr res) {
  res->send_json(buffer_.GetString());
  reset_writer();
}

void Dashboard::reset_writer() {
  buffer_.Clear();
  writer_.Reset(buffer_);
}
