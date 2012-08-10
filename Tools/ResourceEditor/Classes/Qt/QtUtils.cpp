#include "QtUtils.h"
#include "GraphItem.h"

//TODO: change to valid code
QDataStream& operator<<(QDataStream& ostream, const PointerHolder& ph)
{
    const GraphItem *item = ph.GetPointer();

    uint sizeOfPointer = sizeof(item);
    ostream.writeRawData((const char *)&item, sizeOfPointer);
    
    return ostream;
}

QDataStream& operator>>(QDataStream& istream, PointerHolder& ph)
{
    GraphItem *item = NULL;
    uint sizeOfPointer = sizeof(item);
    istream.readRawData((char *)&item, sizeOfPointer);
    
    ph.SetPointer(item);
    return istream;
}


PointerHolder::PointerHolder()
{
    storedPointer = NULL;
}

PointerHolder::PointerHolder(const PointerHolder &fromHolder)
{
    storedPointer = fromHolder.storedPointer;
}

PointerHolder::~PointerHolder()
{
    storedPointer = NULL;
}

void PointerHolder::SetPointer(GraphItem *pointer)
{
    storedPointer = pointer;
}

GraphItem * PointerHolder::GetPointer() const
{
    return storedPointer;
}


QVariant PointerHolder::ToQVariant(GraphItem *item)
{
    PointerHolder holder;
    holder.SetPointer(item);
    
    QVariant variant = QVariant::fromValue(holder);
    return variant;
}

GraphItem * PointerHolder::ToGraphItem(const QVariant &variant)
{
    PointerHolder holder = variant.value<PointerHolder>();
    return holder.GetPointer();
}
