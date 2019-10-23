/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#pragma once

#include <qguiapplication.h>

#include <qsgmaterial.h>
#include <qsgnode.h>

#include <qquickitem.h>
#include <qquickview.h>

#include <qsgsimplerectnode.h>

#include <qsgsimplematerial.h>

//! [1]
struct State
{
    QColor color;

    int compare(const State *other) const
    {
        uint rgb      = color.rgba();
        uint otherRgb = other->color.rgba();

        if (rgb == otherRgb)
        {
            return 0;
        }
        else if (rgb < otherRgb)
        {
            return -1;
        }
        else
        {
            return 1;
        }
    }
};
//! [1]

//! [2]
class Shader : public QSGSimpleMaterialShader<State>
{
    QSG_DECLARE_SIMPLE_COMPARABLE_SHADER(Shader, State)
    //! [2] //! [3]
  public:
    const char *vertexShader() const
    {
        return "attribute highp vec4 aVertex;                              \n"
               "attribute highp vec2 aTexCoord;                            \n"
               "uniform highp mat4 qt_Matrix;                              \n"
               "uniform highp mat4 mvp_matrix;                              \n"
               "varying highp vec2 texCoord;                               \n"
               "void main() {                                              \n"
               "    gl_Position = qt_Matrix * aVertex;                     \n"
               "    texCoord = aTexCoord;                                  \n"
               "}";
    }

    const char *fragmentShader() const
    {
        return "uniform lowp float qt_Opacity;                             \n"
               "uniform lowp vec4 color;                                   \n"
               "varying highp vec2 texCoord;                               \n"
               "void main ()                                               \n"
               "{                                                          \n"
               //"if (cos(0.1*abs(distance(vec2(100.0,100.0).xy, texCoord.xy))) + 0.5 > 0.0) { \n"
               "if(texCoord.x < 0.1 || texCoord.x > 0.2 && texCoord.x < 0.3) { \n"
               "    gl_FragColor = vec4(0.0,0.0,0.0,0.0) * qt_Opacity;  \n"
               " } else {\n"
               "    gl_FragColor = texCoord.y * texCoord.x * color * qt_Opacity;  \n"
               "    }\n"
               "}";
    }
    //! [3] //! [4]
    QList<QByteArray> attributes() const
    {
        return QList<QByteArray>() << "aVertex"
                                   << "aTexCoord";
    }
    //! [4] //! [5]
    void updateState(const State *state, const State *) { program()->setUniformValue(id_color, state->color); }
    //! [5] //! [6]
    void resolveUniforms() { id_color = program()->uniformLocation("color"); }

  private:
    int id_color;
    //! [6]
};

//! [7]
class ColorNode : public QSGGeometryNode
{
  public:
    ColorNode() : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
    {
        setGeometry(&m_geometry);

        QSGSimpleMaterial<State> *material = Shader::createMaterial();
        material->setFlag(QSGMaterial::Blending);
        setMaterial(material);
        setFlag(OwnsMaterial);
    }

    QSGGeometry m_geometry;
};
//! [7]

//! [8]
class Item : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

  public:
    Item() { setFlag(ItemHasContents, true); }

    void setColor(const QColor &color)
    {
        if (m_color != color)
        {
            m_color = color;
            emit colorChanged();
            update();
        }
    }
    QColor color() const { return m_color; }

  signals:
    void colorChanged();

  private:
    QColor m_color;

    //! [8] //! [9]
  public:
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *)
    {
        ColorNode *n = static_cast<ColorNode *>(node);
        if (!node)
            n = new ColorNode();

        QSGGeometry::updateTexturedRectGeometry(n->geometry(), boundingRect(), QRectF(0, 0, 1, 1));
        static_cast<QSGSimpleMaterial<State> *>(n->material())->state()->color = m_color;

        n->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);

        return n;
    }
};

//#include "simplematerial.moc"
//! [11]
