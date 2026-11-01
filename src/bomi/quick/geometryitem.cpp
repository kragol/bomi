#include "geometryitem.hpp"

auto GeometryItem::geometryChange(const QRectF &n, const QRectF &o) -> void
{
    QQuickItem::geometryChange(n, o);
    m_size = n.size();
}
