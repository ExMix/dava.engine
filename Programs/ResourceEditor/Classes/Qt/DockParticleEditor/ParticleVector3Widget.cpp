#include "Classes/Qt/DockParticleEditor/ParticleVector3Widget.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>

#include "Classes/Qt/Tools/EventFilterDoubleSpinBox/EventFilterDoubleSpinBox.h"

ParticleVector3Widget::ParticleVector3Widget(const std::string& label, const DAVA::Vector3& initVector)
{
    QVBoxLayout* boxLayout = new QVBoxLayout(this);
    QHBoxLayout* dataLayout = new QHBoxLayout();

    QGroupBox* groupBox = new QGroupBox();

    groupBox->setTitle(label.c_str());
    groupBox->setCheckable(false);
    groupBox->setLayout(dataLayout);

    QLabel* xLabel = new QLabel("X:");
    QLabel* yLabel = new QLabel("Y:");
    QLabel* zLabel = new QLabel("Z:");

    xSpin = new EventFilterDoubleSpinBox();
    ySpin = new EventFilterDoubleSpinBox();
    zSpin = new EventFilterDoubleSpinBox();

    InitSpinBox(xSpin, initVector.x);
    InitSpinBox(ySpin, initVector.y);
    InitSpinBox(zSpin, initVector.z);

    dataLayout->addWidget(xLabel);
    dataLayout->addWidget(xSpin);

    dataLayout->addWidget(yLabel);
    dataLayout->addWidget(ySpin);

    dataLayout->addWidget(zLabel);
    dataLayout->addWidget(zSpin);

    boxLayout->addWidget(groupBox);
    setLayout(boxLayout);
}

DAVA::Vector3 ParticleVector3Widget::GetValue() const
{
    return { xSpin->value(), ySpin->value(), zSpin->value() };
}

void ParticleVector3Widget::OnValueChanged()
{
    emit valueChanged();
}

void ParticleVector3Widget::InitSpinBox(EventFilterDoubleSpinBox* spin, DAVA::float32 value)
{
    spin->setMinimum(-100000000000000000000.0);
    spin->setMaximum(100000000000000000000.0);
    spin->setSingleStep(0.001);
    spin->setDecimals(4);
    spin->setValue(value);
    connect(spin, SIGNAL(valueChanged(double)), this, SLOT(OnValueChanged()));
    spin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}
