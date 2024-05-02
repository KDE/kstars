#include "camera.h"

namespace Ekos
{

Camera::Camera(QWidget *parent)
    : QWidget{parent}
{
    setupUi(this);
}
} // namespace Ekos
