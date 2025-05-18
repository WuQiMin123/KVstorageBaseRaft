#pragma once
#include <google/protobuf/service.h>
#include <string>


//提供额外的控制和状态管理能力
class MprpcController:public google::protobuf::RpcController{
public:
    MprpcController();
    void Reset();
    bool Failed() const;
    std::string ErrorTex() const;
    void SetFailed(const std::string& reason);

    void StartCancel();
    bool IsCanceled() const;
    void NotifyOnCancel(google::protobuf::Closure* callback);

private:
    bool m_failed;
    std::string m_errText;
}