#include <iostream>
#include <fstream>

#include "System.h"
#include "AHRSEKF.h"
#include "AHRSEKF2.h"
#include "AHRSESKF.h"
#include "SensorData.h"
#include "Converter.h"

namespace RAIN_IMU
{

System::System()
{
}


System::~System()
{
}

int System::RunEKF()
{
	AHRSEKF ekf;
	unsigned int index = 0;
	const double T = 0.02;
	
	ekf.ReadSensorData();

	Eigen::Vector3d eulerinit = ekf.Initialize(ekf.GetSensordatabyID(0,false));
	
	Eigen::Quaterniond quatinit = Converter::euler2quat(eulerinit);

	Eigen::Quaterniond q = quatinit;
	Eigen::Quaterniond q1;
	Eigen::Matrix<double, 4, 4> P, P1;
	Eigen::Matrix<double, 3, 4> Hk2;
	Eigen::Matrix<double, 3, 1> hk2;

	// initialize the P prior matrix, the initialization value is not very clear

	ekf.initalizevarMatrix(P);
 
	SensorData sensordata;
	SensorData sensordatanorm;
	while(1)
	{
		sensordata = ekf.GetSensordatabyID(index,false);
		sensordatanorm = ekf.GetSensordatabyID(index,true);

		index++;
		
		// start of "a prior" system estimation:
		Eigen::Matrix<double, 4, 4> OmegaMatrix = Converter::BigOmegaMatrix(Eigen::Vector3d(sensordata.Gyro.X, sensordata.Gyro.Y, sensordata.Gyro.Z));

		Eigen::Matrix<double, 4, 4> Ak = ekf.DiscreteTime(OmegaMatrix, T);
	
		q = Converter::vector4d2quat(Ak * Converter::quat2vector4d(q));
		Converter::quatNormalize(q);

		Eigen::Matrix<double, 4, 4> Qk = (double)1e-6 *  Eigen::MatrixXd::Identity(4, 4);

		P = Ak * P * Ak.transpose() + Qk;

		// Start of the correction stage 1:
		Eigen::Matrix<double, 3, 4> Hk1 = -ekf.JacobianHk1Matrix(q); // get from the quaternion

		Eigen::Matrix<double, 3, 3> Vk = Eigen::MatrixXd::Identity(3, 3);
		Eigen::Matrix<double, 3, 3> R1 = 2 * Eigen::MatrixXd::Identity(3, 3);

		Eigen::Matrix<double, 4, 3> Kk1 = P * Hk1.transpose() * (Hk1 * P * Hk1.transpose() + Vk * R1 * Vk.transpose()).inverse();

		Eigen::Matrix<double, 3, 1> h1 = ekf.Calculateh1Matrix(q);
		Eigen::Vector3d zk1(sensordatanorm.Acc.X, sensordatanorm.Acc.Y, sensordatanorm.Acc.Z);

		Eigen::Vector4d vq1 = Kk1 * (zk1 + h1);

		vq1[3] = 0;

		q1 = Converter::quatplusquat(q, Converter::vector4d2quat(vq1));

		Converter::quatNormalize(q);

		P1 = (Eigen::MatrixXd::Identity(4, 4) - Kk1 * Hk1) *  P;

		// Start fo the correction stage 2:
		ekf.CalcObservationMatrix(q,Hk2,hk2,sensordatanorm,T);

		Eigen::Matrix<double, 3, 3> Vk2 = Eigen::MatrixXd::Identity(3, 3);
		Eigen::Matrix<double, 3, 3> R2 = 1 * Eigen::MatrixXd::Identity(3, 3);
		Eigen::Matrix<double, 4, 3> Kk2 = P*Hk2.transpose()*(Hk2*P*Hk2.transpose() + Vk2*R2*Vk2.transpose()).inverse(); //Vk2*R2*Vk2.transpose() 

		Eigen::Vector3d zk2(sensordatanorm.Mag.X, sensordatanorm.Mag.Y, sensordatanorm.Mag.Z);

		Eigen::Vector4d vq2 = Kk2 * (zk2 - hk2);
		Converter::quatNormalize(Converter::vector4d2quat(vq2));
		vq2[1] = 0;
		vq2[2] = 0;

		q = Converter::quatplusquat(q1, Converter::vector4d2quat(vq2));
		Converter::quatNormalize(q);

		P = (Eigen::MatrixXd::Identity(4, 4) - Kk2 * Hk2) *  P1;
 
		// yaw pitch roll
		Eigen::Vector3d euler = Converter::quat2euler(q); 
 		euler[0] = euler[0] * ekf.RAD_DEG - 8.3;
		euler[1] = euler[1] * ekf.RAD_DEG;
		euler[2] = euler[2] * ekf.RAD_DEG;
		std::cout << euler[0] << " " << euler[1] << " " << euler[2] << std::endl;

		if (index == 100)
			return 0;
	}


}

int System::RunEKF2()
{
	AHRSEKF2 ekf;
	const double T = 0.02;
	unsigned int index = 0;
	SensorData sensordatanormk;
	SensorData sensordataunnormk;

	ekf.ReadSensorData();

	Eigen::Matrix<double, 1, 7> x = Eigen::MatrixXd::Zero(1, 7);
	Eigen::Matrix<double, 1, 7> x_ = Eigen::MatrixXd::Zero(1, 7);
	Eigen::Matrix<double, 1, 6> z;

	Eigen::Matrix<double, 7, 7> Pk_ = Eigen::MatrixXd::Zero(7, 7);
	Eigen::Matrix<double, 7, 7> Pk = Eigen::MatrixXd::Identity(7, 7);
	Eigen::Matrix<double, 1, 6> hk;
	Eigen::Matrix<double, 6, 7> Hk = Eigen::MatrixXd::Zero(6, 7);
	Eigen::Matrix<double, 7, 6> Kk = Eigen::MatrixXd::Zero(7, 6);

	Eigen::Matrix<double, 6, 6> R;
	Eigen::Matrix<double, 7, 7> Q;
	Eigen::Matrix<double, 7, 7> Ak;

	Eigen::Vector3d euler;
	Eigen::Quaterniond qfilter;

	EulerAngle eulerinit;
	eulerinit = ekf.InitializeEuler(ekf.GetSensordatabyID(0,false));

	Eigen::Quaterniond qinit = Converter::euler2quat(Eigen::Vector3d(eulerinit.Yaw,eulerinit.Pitch,eulerinit.Roll));

	ekf.InitializeVarMatrix(R,Q);

	x[0] = qinit.w(), x[1] = qinit.x(), x[2] = qinit.y(), x[3] = qinit.z();
	while(1)
	{
		//  the sensordatak that have the normalized, if want to use the unnormalized data, set the false flag in the GetSensordata function  
		sensordatanormk = ekf.GetSensordatabyID(index,true);
		sensordataunnormk = ekf.GetSensordatabyID(index,false);

		ekf.FillObserveState(z,sensordatanormk);

		ekf.UpdateState(x,x_,sensordataunnormk,T);

		ekf.FillTransiteMatrix(Ak, sensordataunnormk, x, T);

		Pk_ = Ak * Pk * Ak.transpose() + Q;

		ekf.FillObserveMatrix(x_,hk,Hk,sensordatanormk);

		Kk = Pk_ * Hk.transpose() * (Hk * Pk_ * Hk.transpose() + R).inverse();

		qfilter = Converter::vector4d2quat(x.block<1,4>(0, 0));

		std::cout.precision(10);
		euler = Converter::quat2euler(qfilter);// yaw pitch roll
		euler[0] = euler[0] * ekf.RAD_DEG  - 8.3;
		euler[1] = euler[1] * ekf.RAD_DEG;
		euler[2] = euler[2]*ekf.RAD_DEG;
		std::cout << "euler:" << euler.transpose() << std::endl;
		sensordatanormk.EulerGroundTruth.Yaw *= ekf.RAD_DEG;
		sensordatanormk.EulerGroundTruth.Pitch *= ekf.RAD_DEG;
		sensordatanormk.EulerGroundTruth.Roll *= ekf.RAD_DEG;
		std::cout << "truth"  << sensordatanormk.EulerGroundTruth.Yaw << " " << sensordatanormk.EulerGroundTruth.Pitch << " "  << sensordatanormk.EulerGroundTruth.Roll << std::endl; 

		x = (x_.transpose() + Kk * (z - hk).transpose()).transpose();

		Converter::Normalize(x);

		Pk = (Eigen::MatrixXd::Identity(7, 7) - Kk * Hk) * Pk_;

		index++;
		if (index == 1000)
			return 0;
	}

	
}

int System::RunESKF()
{
	AHRSESKF eskf;
	Eigen::Vector3d eulerinit;
	Eigen::Quaterniond quatinit;
	Eigen::Matrix<double, 6, 6> Q, Qi, R;
	Eigen::Matrix<double, 6, 6> PPrior,P;
	Eigen::Matrix<double, 6, 6> Fx;
	Eigen::Matrix<double, 6, 6> Fi = Eigen::MatrixXd::Identity(6, 6);
	Eigen::Matrix<double, 6, 6> Hk;
	Eigen::Matrix<double, 1, 6> hk, z;
	Eigen::Matrix<double, 6, 6> K;
	Eigen::Matrix<double, 1, 6> vdetx;
	Eigen::Quaterniond detq;
	ErrorState detx;

	const double T = 0.02;

	unsigned int index = 1;

	eskf.ReadSensorData();
	eskf.InitializeVarMatrix(Q, R, P); // covariance estimate 
	eulerinit = eskf.Initialize(eskf.GetSensordatabyID(0,false));
	quatinit = Converter::euler2quat(eulerinit);

	eskf.NominalStates.q = quatinit;

	eskf.vSensorData.at(0).CalculateEuler.Yaw = eulerinit[0] * eskf.RAD_DEG;
	eskf.vSensorData.at(0).CalculateEuler.Pitch = eulerinit[1] * eskf.RAD_DEG;
	eskf.vSensorData.at(0).CalculateEuler.Roll = eulerinit[2] * eskf.RAD_DEG;

	SensorData sensordata;
	SensorData sensordata2;
	SensorData sensordatanorm;
	while(1)
	{
		// sensor data 
		sensordata = eskf.GetSensordatabyID(index,false);
		sensordata2 = eskf.GetSensordatabyID((index+1),false);
		sensordatanorm = eskf.GetSensordatabyID(index,true);

		// prior nominal state 
		eskf.PredictNominalState(sensordata, sensordata2, T); // OK

		Fx = eskf.CalcTransitionMatrix(sensordata, T); // OK

		// there is no difference whether running this function
		eskf.PredictErrorState(Fx);

		// predict prior covariance estimate 
		Qi = Q * T;
		P = Fx * P * Fx.transpose() + Fi * Qi * Fi.transpose(); // OK

		eskf.EnforcePSD(P);

		// the sensor data have been normalizd, it is very important
		eskf.CalcObservationMatrix(Hk,hk,sensordatanorm,T);

		K = P*Hk.transpose() * (Hk*P*Hk.transpose() + R).inverse();

		eskf.ObserveValue(z,sensordatanorm);

		vdetx = K * (z + hk).transpose();
		eskf.ErrorStates.det_theta = vdetx.block<1, 3>(0, 0);
		eskf.ErrorStates.det_wb = vdetx.block<1, 3>(0, 3);

		P = P - K*(Hk*P*Hk.transpose() + R)*K.transpose();

		// integrate error state to the nominal state
		detq = eskf.BuildUpdateQuat(eskf.ErrorStates);

		eskf.NominalStates.q = Converter::vector4d2quat(Converter::quatleftproduct(eskf.NominalStates.q) * Converter::quat2vector4d(detq));

		Converter::quatNormalize(eskf.NominalStates.q);

		eskf.NominalStates.wb = eskf.NominalStates.wb + eskf.ErrorStates.det_wb;

		// reset the error state
		eskf.ErrorStates.det_theta = Eigen::MatrixXd::Zero(3, 1);
		eskf.ErrorStates.det_wb = Eigen::MatrixXd::Zero(3, 1);
		P = Eigen::MatrixXd::Identity(6, 6)*P*Eigen::MatrixXd::Identity(6, 6).transpose();

		// display 
		Eigen::Vector3d euler = Converter::quat2euler(eskf.NominalStates.q);
		euler[0] = euler[0] * eskf.RAD_DEG  - 8.3;
		euler[1] = euler[1] * eskf.RAD_DEG;
		euler[2] = euler[2] * eskf.RAD_DEG;
		//std::cout << "euler:" << euler.transpose() << std::endl;
		sensordatanorm.EulerGroundTruth.Yaw *= eskf.RAD_DEG;
		sensordatanorm.EulerGroundTruth.Pitch *= eskf.RAD_DEG;
		sensordatanorm.EulerGroundTruth.Roll *= eskf.RAD_DEG;
		//std::cout << "truth"  << sensordatanorm.EulerGroundTruth.Yaw << " " << sensordatanorm.EulerGroundTruth.Pitch << " "  << sensordatanorm.EulerGroundTruth.Roll << std::endl; 
		eskf.vSensorData.at(index).CalculateEuler.Yaw = euler[0];
		eskf.vSensorData.at(index).CalculateEuler.Pitch = euler[1];
		eskf.vSensorData.at(index).CalculateEuler.Roll = euler[2];
		eskf.vSensorData.at(index).EulerGroundTruth.Yaw = sensordatanorm.EulerGroundTruth.Yaw;
		eskf.vSensorData.at(index).EulerGroundTruth.Pitch = sensordatanorm.EulerGroundTruth.Pitch;
		eskf.vSensorData.at(index).EulerGroundTruth.Roll = sensordatanorm.EulerGroundTruth.Roll;

		index++;
	
		if(index == (4200 - 1))
		{
			std::cout << "finish the calculation" << std::endl;
			SaveData(eskf.vSensorData);
			std::cout << "sava data successfully" << std::endl;

			return 0;
		}
			
	}

	return 0;
}

int System::SaveData(std::vector<SensorData> vSensorData)
{
	std::ofstream outfile;
	long unsigned int index = 0;;
	outfile.open("log.txt");
	if (!outfile)
	{
		std::cout << "sava data fail" << std::endl;
		return 0;
	}

	while(index < (4200-2))
	{
		outfile << index;
		outfile << " ";
		outfile << vSensorData.at(index).CalculateEuler.Yaw;
		outfile << " ";
		outfile << vSensorData.at(index).CalculateEuler.Pitch;
		outfile << " ";
		outfile << vSensorData.at(index).CalculateEuler.Roll;
		outfile << " ";
		outfile << vSensorData.at(index).EulerGroundTruth.Yaw;
		outfile << " ";
		outfile << vSensorData.at(index).EulerGroundTruth.Pitch;
		outfile << " ";
		outfile << vSensorData.at(index).EulerGroundTruth.Roll;
		outfile << std::endl;

		index++;

	}

	outfile.close();

	return 0;
}

}