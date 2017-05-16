#include "Converter.h"
#include "SensorData.h"
namespace RAIN_IMU
{
/**
*	Hamilton (q_w,q_v) Right hand
*  Q = q_w+q_x*i+q_y*j+q_z*k	i * j = k
*  vector: q = [q_w q_v] = [q_w q_x q_y q_z]
*	x_g = q * x_l * q_*
*	euler��z y x�� 
*		   yaw pitch roll 
*
**/

Converter::Converter()
{
}


Converter::~Converter()
{
}

/**
 * euler[0] euler[1] euler[2]
 * yaw		pitch	 roll 
 *
**/
Eigen::Quaterniond Converter::euler2quat(const Eigen::Vector3d &euler)
{
	double cYaw = cos(euler[0] / 2), sYaw = sin(euler[0] / 2);
	double cPit = cos(euler[1] / 2), sPit = sin(euler[1] / 2);
	double cRol = cos(euler[2] / 2), sRol = sin(euler[2] / 2);
	
	Eigen::Quaterniond q;

	q.w() = cRol*cPit*cYaw + sRol*sPit*sYaw;
	q.x() = sRol*cPit*cYaw - cRol*sPit*sYaw;
	q.y() = cRol*sPit*cYaw + sRol*cPit*sYaw;
	q.z() = cRol*cPit*sYaw - sRol*sPit*cYaw;

	return q;
}

Eigen::Vector3d Converter::quat2euler(const Eigen::Quaterniond &q)
{
	double yaw, pitch, roll;
	double r1 = 2*(q.w()*q.x() + q.y()*q.z());
	double r2 = 1 - 2*(q.x()*q.x() + q.y()*q.y());
	double r3 = 2*(q.w()*q.y() - q.z()*q.x());
	double r4 = 2*(q.w()*q.z() + q.x()*q.y());
	double r5 = 1 - 2*(q.y()*q.y() + q.z()*q.z());

	roll = atan2(r1, r2);
	pitch = asin(r3);
	yaw = atan2(r4,r5);

	Eigen::Vector3d euler(yaw,pitch,roll);

	return euler;
}
void Converter::quatNormalize(Eigen::Quaterniond &q)
{
	double norm = sqrt(q.w()*q.w() + q.x()*q.x() + q.y()*q.y() + q.z()*q.z());
	q.w() = q.w() / norm;
	q.x() = q.x() / norm;
	q.y() = q.y() / norm;
	q.z() = q.z() / norm;
}

Eigen::Quaterniond Converter::quatMultiquat(const Eigen::Quaterniond &q1,const Eigen::Quaterniond &q2)
{
	Eigen::Quaterniond q;
	q.w() = q1.w()*q2.w() - q1.x()*q2.x() - q1.y()*q2.y() - q1.z()*q2.z();
	q.x() = q1.w()*q2.x() + q1.x()*q2.w() + q1.y()*q2.z() - q1.z()*q2.y();
	q.y() = q1.w()*q2.y() - q1.x()*q2.z() + q1.y()*q2.w() + q1.z()*q2.x();
	q.z() = q1.w()*q2.z() + q1.x()*q2.y() - q1.y()*q2.x() + q1.z()*q2.w();

	return q;
}

Eigen::Matrix<double, 4, 4> Converter::quatleftproduct(const Eigen::Quaterniond &q0)
{
	Eigen::Matrix<double, 4, 4> qL;

	qL << q0.w(), -q0.x(), -q0.y(), -q0.z(),
		  q0.x(),  q0.w(), -q0.z(),  q0.y(),
		  q0.y(),  q0.z(),  q0.w(), -q0.x(),
		  q0.z(), -q0.y(),  q0.x(),  q0.w();
	
	return qL;
}

Eigen::Matrix<double, 4, 4> Converter::quatRightproduct(const Eigen::Quaterniond &q0)
{
	Eigen::Matrix<double, 4, 4> qR;

	qR << q0.w(), -q0.x(), -q0.y(), -q0.z(),
		  q0.x(),  q0.w(),  q0.z(), -q0.y(),
		  q0.y(), -q0.z(),  q0.w(),  q0.x(),
		  q0.z(),  q0.y(), -q0.x(),  q0.w();

	return qR;
}

Eigen::Matrix<double, 3, 3> Converter::quat2rotmatrix(const Eigen::Quaterniond &q0)
{
	Eigen::Matrix<double, 3, 3> R;

	double qwqw = q0.w()*q0.w(), qxqx = q0.x()*q0.x(), qyqy = q0.y()*q0.y(), qzqz = q0.z()*q0.z();
	double qxqy = q0.x()*q0.y(), qxqz = q0.x()*q0.z();
	double qyqz = q0.y()*q0.z();
	double qwqx = q0.w()*q0.x(), qwqz = q0.w()*q0.z(), qwqy = q0.w()*q0.y();

	R << qwqw + qxqx - qyqy - qzqz, 2 * (qxqy - qwqz), 2 * (qxqz + qwqy),
		 2 * (qxqy + qwqz), qwqw - qxqx + qyqy - qzqz, 2 * (qyqz - qwqx),
		 2 * (qxqz - qwqy), 2 * (qyqz + qwqx), qwqw - qxqx - qyqy + qzqz;
	
	return R;
}

Eigen::Matrix<double, 4, 4> Converter::OmegaMatrix(const SensorData &sensordata)
{
	double Gyro_x, Gyro_y, Gyro_z;
	Gyro_x = sensordata.Gyro.X;
	Gyro_y = sensordata.Gyro.Y;
	Gyro_z = sensordata.Gyro.Z;

	Eigen::Matrix<double, 4, 4> OmegaMatrix;
	OmegaMatrix << 0, -Gyro_x, -Gyro_y, -Gyro_z,
				   Gyro_x, 0, Gyro_z, -Gyro_y,
				   Gyro_y, -Gyro_z, 0, Gyro_x,
				   Gyro_z, Gyro_y, -Gyro_x, 0;
	
	return OmegaMatrix;
}



}

