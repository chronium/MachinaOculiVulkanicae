#include <SDL2/SDL.h>

#include <mov/Application.hpp>
#include <mov/backend/VulkanInstance.hpp>

class TestApp final : public mov::Application
{
public:
  explicit TestApp(const mov::ApplicationCreateInfo& create_info)
    : Application(create_info)
  {
  }

  ~TestApp() override = default;

  TestApp(TestApp&) = delete;
  TestApp(TestApp &&) = delete;

  void operator=(TestApp&) = delete;
  void operator=(TestApp&&) = delete;

  void render() override;
  void init() override;
};

void TestApp::init()
{
}

void TestApp::render()
{
}

int main(int argc, char *argv[])
{
  return TestApp({"Machina Oculi Vulkanicae", 0, 1, 0, 1920, 1080}).run();
}
