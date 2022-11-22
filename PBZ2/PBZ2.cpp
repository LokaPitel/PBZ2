#include <iostream>
#include <ctime>
#include <mysqlx/xdevapi.h>

class Model
{
    mysqlx::Session* sess;
    mysqlx::Schema* db;

    mysqlx::Table* additionalPartDocument = NULL;
    mysqlx::Table* employee = NULL;
    mysqlx::Table* employeeTransitionHistory = NULL;
    mysqlx::Table* equipmentRepair = NULL;
    mysqlx::Table* equipment = NULL;
    mysqlx::Table* equipmentTransitionHistory = NULL;
    mysqlx::Table* deletedEmployees = NULL;


public:
    Model(std::string ip, int port, std::string username, std::string password, std::string schemeName)
    {
        sess = new mysqlx::Session(ip, port, username, password);
        db = new mysqlx::Schema(sess->getSchema(schemeName));

        additionalPartDocument = new mysqlx::Table(db->getTable("additional_part_document"));
        employee = new mysqlx::Table(db->getTable("employee"));
        employeeTransitionHistory = new mysqlx::Table(db->getTable("employee_transition_history"));
        equipmentRepair = new mysqlx::Table(db->getTable("equipment_repair"));
        equipment = new mysqlx::Table(db->getTable("equipment"));
        equipmentTransitionHistory = new mysqlx::Table(db->getTable("equipment_transition_history"));
        deletedEmployees = new mysqlx::Table(db->getTable("deleted_employees"));
    }

    ~Model()
    {
        delete sess;
        delete db;
        delete additionalPartDocument;
        delete employee;
        delete employeeTransitionHistory;
        delete equipmentRepair;
        delete equipment;
        delete equipmentTransitionHistory;
        delete deletedEmployees;
    }

    std::string getFromEquipment(std::string e_id, std::string attrName)
    {
        sess->startTransaction();

        try
        {
            mysqlx::RowResult info = equipment->select(attrName)
                .where("e_id = " + std::string("\"") + e_id + std::string("\"")).execute();

            return std::string(info.fetchOne().get(0));
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }

        sess->commit();
    }

    void setInEquipment(std::string e_id, std::string attrName, std::string value)
    {
        sess->startTransaction();

        try
        {
            equipment->update()
                .set(attrName, value)
                .where("e_id = " + std::string("\"") + e_id + std::string("\"")).execute();

            sess->commit();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }
    }

    void addEquipmentInformation(std::string date, std::string name, std::string modelName, std::string departament, std::string dateOfShipment)
    {
        sess->startTransaction();

        try
        {
            mysqlx::RowResult ids = equipment->select("e_id").execute();

            int nextId = -1;

            if (ids.count() == 0)
                nextId = 1;

            else
            {
                int tmp;

                for (mysqlx::Row item : ids.fetchAll())
                {
                    std::string id = std::string(item.get(0));
                    std::stringstream ss(id.substr(1, id.length()));

                    ss >> tmp;

                    if (tmp > nextId)
                        nextId = tmp;
                }

                nextId++;
            }

            std::string idRepr = "E" + std::to_string(nextId);

            equipment->insert("e_id", "ManufacturingDate", "Name", "Model", "Departament")
                .values(idRepr, date, name, modelName, departament).execute();

            equipmentTransitionHistory->insert("e_id", "date", "Departament")
                .values(idRepr, dateOfShipment, departament).execute();

            sess->commit();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }
    }
    
    void editEquipmentDepartament(std::string e_id, std::string departament)
    {
       /* sess->setSavepoint();
        try
        {*/
        sess->startTransaction();

        try
        {
            equipment->update()
                .set("Departament", departament)
                .where("e_id = " + std::string("\"") + e_id + std::string("\"")).execute();

            std::string date = std::string(equipmentTransitionHistory->select("DATE_FORMAT(date, \"%Y-%m-%d\")")
                .where("e_id = " + std::string("\"") + e_id + std::string("\"")).orderBy("date DESC").execute().fetchOne().get(0));

            equipmentTransitionHistory->update()
                .set("Departament", departament)
                .where("e_id = " + std::string("\"") + e_id + std::string("\"") + std::string(" AND date = ") + std::string("\"") + date + std::string("\""));
       
            sess->commit();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }
    }

    void editEquipmentInformation(std::string e_id, std::string date, std::string name, std::string modelName, std::string departament)
    {
        sess->startTransaction();

        try
        {
            equipment->update()
                .set("ManufacturingDate", date)
                .set("Name", name)
                .set("Model", modelName)

                .where("e_id = " + std::string("\"") + e_id + std::string("\"")).execute();

            editEquipmentDepartament(e_id, departament);
            sess->commit();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }
    }

    void deleteEquipmentInformation(std::string e_id)
    {
        sess->startTransaction();

        try
        {
            equipment->remove()
                .where("e_id = " + std::string("\"") + e_id + std::string("\"")).execute();

            equipmentTransitionHistory->remove()
                .where("e_id = " + std::string("\"") + e_id + std::string("\"")).execute();

            deleteEquipmentToReapir(e_id);

            sess->commit();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }
    }

    void transiteEquipmentToAnotherDepartament(std::string e_id, std::string date, std::string departament)
    {
        sess->startTransaction();

        try
        {
            if (getFromEquipment(e_id, "Departament") != departament)
            {
                equipmentTransitionHistory->insert("e_id", "date", "Departament")
                    .values(e_id, date, departament).execute();

                setInEquipment(e_id, "Departament", departament);
            }

            sess->commit();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }
    }

    // Employees
    std::string getFromEmployee(std::string p_id, std::string attrName)
    {
        sess->startTransaction();

        try
        {
            mysqlx::RowResult info = employee->select(attrName)
                .where("p_id = " + std::string("\"") + p_id + std::string("\"")).execute();

            return std::string(info.fetchOne().get(0));
            sess->commit();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }
    }

    void setInEmployee(std::string p_id, std::string attrName, std::string value)
    {
        sess->startTransaction();

        try
        {
            employee->update()
                .set(attrName, value)
                .where("p_id = " + std::string("\"") + p_id + std::string("\"")).execute();

            sess->commit();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }
    }

    void addEmployeeInformation(std::string surname, std::string name, std::string fathername, std::string departament, std::string role, std::string dateOfEnrollment)
    {
        sess->startTransaction();

        try
        {
            mysqlx::RowResult ids = employee->select("p_id").execute();

            int nextId = -1;

            if (ids.count() == 0)
                nextId = 1;

            else
            {
                int tmp;

                for (mysqlx::Row item : ids.fetchAll())
                {
                    std::string id = std::string(item.get(0));
                    std::stringstream ss(id.substr(1, id.length()));

                    ss >> tmp;

                    if (tmp > nextId)
                        nextId = tmp;
                }

                nextId++;
            }

            std::string idRepr = "P" + std::to_string(nextId);

            employee->insert("p_id", "Surname", "Name", "FatherName", "Departament", "Role")
                .values(idRepr, surname, name, fathername, departament, role).execute();

            employeeTransitionHistory->insert("p_id", "Departament", "Date")
                .values(idRepr, departament, dateOfEnrollment).execute();

            sess->commit();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }
    }
  
    void editEmployeeDepartament(std::string p_id, std::string departament)
    {
        sess->startTransaction();
        try
        {
            employee->update()
                .set("Departament", departament)
                .where("p_id = " + std::string("\"") + p_id + std::string("\"")).execute();

            std::string date =  std::string(employeeTransitionHistory->select("DATE_FORMAT(Date, \"%Y-%m-%d\")")
                .where("p_id = " + std::string("\"") + p_id + std::string("\"")).orderBy("Date DESC").execute().fetchOne().get(0));

            employeeTransitionHistory->update()
                .set("Departament", departament)
                .where("p_id = " + std::string("\"") + p_id + std::string("\"") + std::string(" AND Date = ") + std::string("\"") + date + std::string("\"")).execute();
        
            sess->commit();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }
    }

    void editEmployeeInformation(std::string p_id, std::string surname, std::string name, std::string fathername, std::string departament, std::string role, std::string dateOfEnrollment)
    {
        sess->startTransaction();

        try
        {
            employee->update()
                .set("Surname", surname)
                .set("Name", name)
                .set("FatherName", fathername)
                .set("Role", role)

                .where("p_id = " + std::string("\"") + p_id + std::string("\"")).execute();

            editEmployeeDepartament(p_id, departament);

            sess->commit();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }
    }

    void deleteEmployeeInformation(std::string p_id)
    {
        sess->startTransaction();

        try
        {
            /*employee->remove()
                .where("p_id = " + std::string("\"") + p_id + std::string("\"")).execute();*/

            deletedEmployees->insert("p_id")
                .values(p_id).execute();
            /*employeeTransitionHistory->remove()
                .where("p_id = " + std::string("\"") + p_id + std::string("\"")).execute();*/

            sess->commit();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }
    }

    void transiteEmployeeToAnotherDepartament(std::string p_id, std::string date, std::string departament)
    {
        sess->startTransaction();

        try
        {
            if (getFromEmployee(p_id, "Departament") != departament)
            {
                employeeTransitionHistory->insert("p_id", "Date", "Departament")
                    .values(p_id, date, departament).execute();

                setInEmployee(p_id, "Departament", departament);
            }

            sess->commit();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }
    }

    void addEquipmentToRepair(std::string e_id, std::string date, std::string type, int repairTime,
        std::string querier, std::string applier, std::string repairer)
    {
        sess->startTransaction();

        try
        {
            equipmentRepair->insert("e_id", "date", "RepairmentType", "RepairmentTime", "Querier", "Applier", "Repairer",
                "AdditionalPart", "Sum")

                .values(e_id, date, type, repairTime, querier, applier, repairer, nullptr, nullptr).execute();

            sess->commit();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }
    }

    void editEquipmentToRepair(std::string e_id, std::string date, std::string type, int repairTime,
        std::string querier, std::string applier, std::string repairer)
    {
        sess->startTransaction();

        try
        {
            equipmentRepair->update()
                .set("RepairmentType", type)
                .set("RepairmentTime", repairTime)
                .set("Querier", querier)
                .set("Applier", applier)
                .set("Repairer", repairer)
                .where(std::string("e_id = ") + std::string("\"") + e_id + std::string("\"") + std::string(" AND ") +
                    std::string("date = ") + std::string("\"") + date + std::string("\""))
                .execute();

                sess->commit();
                //.set("Sum", sum)
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }
    }

    void setEquipmentToRepairDocument(std::string e_id, std::string date, std::string a_id)
    {
        sess->startTransaction();

        try
        {
            double sum = 0.;

            mysqlx::RowResult prices = additionalPartDocument->select("Price")
                .where(std::string("a_id = ") + std::string("\"") + a_id + std::string("\""))
                .execute();

            for (auto price : prices.fetchAll())
            {
                sum += std::stod(std::string(price.get(0)));
            }

            equipmentRepair->update()
                .set("AdditionalPart", a_id)
                .set("Sum", sum)
                .where(std::string("e_id = ") + std::string("\"") + e_id + std::string("\"") + std::string(" AND ") +
                    std::string("date = ") + std::string("\"") + date + std::string("\""))
                .execute();

            sess->commit();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }
    }

    void deleteEquipmentToReapir(std::string e_id)
    {
        sess->startTransaction();

        try
        {
            mysqlx::RowResult toDelete = equipmentRepair->select("AdditionalPart")
                .where("e_id = " + std::string("\"") + e_id + std::string("\"")).execute();

            for (auto value : toDelete.fetchAll())
            {
                std::string a_id = std::string(value.get(0));

                deleteAdditionalPartDocument(a_id);
            }

            equipmentRepair->remove().where("e_id = " + std::string("\"") + e_id + std::string("\""));
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }

        sess->commit();
    }

    void addAdditionalPartDocument(std::string a_id, std::string additionalPartName, std::string date, double price)
    {
        sess->startTransaction();

        try
        {
            additionalPartDocument->insert("a_id", "AdditionalPartName", "Date", "Price")
                .values(a_id, additionalPartName, date, price).execute();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }

        sess->commit();
    }

    void editAdditionalPartDocument(std::string a_id, std::string additionalPartName, std::string date, double price)
    {
        sess->startTransaction();

        try
        {
            additionalPartDocument->update()
                .set("Price", price)
                .where(std::string("a_id = ") + std::string("\"") + a_id + std::string("\"") + std::string(" AND Date = ") +
                    std::string("\"") + additionalPartName + std::string("\"") + std::string(" AND AdditionalPartName = ") +
                    std::string("\"") + date + std::string("\""))
                .execute();
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }

        sess->commit();
    }

    void deleteAdditionalPartDocument(std::string a_id)
    {
        sess->startTransaction();

        try
        {
            additionalPartDocument->remove().where("a_id = " + std::string("\"") + a_id + std::string("\""));
        }

        catch (mysqlx::Error err)
        {
            std::cerr << "ERROR: " << err.what();
            sess->rollback();
        }

        sess->commit();
    }
};

class View
{

};

class Controller
{

};

class Application
{
public:
    void run()
    {   
        Model model("localhost", 33060, "root", "peta_loka", "equipmentrepair");

        //model.deleteEmployeeInformation("P1");
        /*model.addEmployeeInformation("Fork", "Os", "Semaphorevic", "IT", "System programmer", "2022-11-14");
        model.addEmployeeInformation("Rihter", "John", "Windowsovic", "IT", "Lead system programmer", "2021-10-14");
        model.addEmployeeInformation("White", "Max", "Stevenson", "HR", "Lead HR", "2020-2-5")*/

        /*model.addEquipmentInformation("2015-10-5", "Freezer", "Athlon 4040", "IT", "2020-5-1");
        model.addEquipmentInformation("2018-4-5", "TV", "EnjoyDeviceM41", "IT", "2020-5-1");
        model.addEquipmentInformation("2020-5-5", "WaterCooler", "W554", "HR", "2020-5-12");*/

        //model.addEquipmentToRepair("E1", "2022-10-17", "Partial", 14, "P2", "P2", "P1");
        //model.addAdditionalPartDocument("A1", "Door", "2022-10-19", 10.5);

        model.setEquipmentToRepairDocument("E1", "2022-10-17", "A1");
    }
};

int main()
{
    Application app;

    app.run();
}