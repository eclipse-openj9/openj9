package common

import (
	prometheusv1 "github.com/coreos/prometheus-operator/pkg/apis/monitoring/v1"
	certmngrv1alpha2 "github.com/jetstack/cert-manager/pkg/apis/certmanager/v1alpha2"
	routev1 "github.com/openshift/api/route/v1"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/runtime"
)

// StatusConditionType ...
type StatusConditionType string

// StatusCondition ...
type StatusCondition interface {
	GetLastTransitionTime() *metav1.Time
	SetLastTransitionTime(*metav1.Time)

	GetLastUpdateTime() metav1.Time
	SetLastUpdateTime(metav1.Time)

	GetReason() string
	SetReason(string)

	GetMessage() string
	SetMessage(string)

	GetStatus() corev1.ConditionStatus
	SetStatus(corev1.ConditionStatus)

	GetType() StatusConditionType
	SetType(StatusConditionType)
}

// BaseComponentStatus returns base appplication status
type BaseComponentStatus interface {
	GetConditions() []StatusCondition
	GetCondition(StatusConditionType) StatusCondition
	SetCondition(StatusCondition)
	NewCondition() StatusCondition
	GetConsumedServices() ConsumedServices
	SetConsumedServices(ConsumedServices)
	GetResolvedBindings() []string
	SetResolvedBindings([]string)
	GetImageReference() string
	SetImageReference(string)
}

// ConsumedServices stores status of the service binding dependencies
type ConsumedServices map[ServiceBindingCategory][]string

const (
	// StatusConditionTypeReconciled ...
	StatusConditionTypeReconciled StatusConditionType = "Reconciled"

	// StatusConditionTypeDependenciesSatisfied ...
	StatusConditionTypeDependenciesSatisfied StatusConditionType = "DependenciesSatisfied"
)

// BaseComponentAutoscaling represents basic HPA configuration
type BaseComponentAutoscaling interface {
	GetMinReplicas() *int32
	GetMaxReplicas() int32
	GetTargetCPUUtilizationPercentage() *int32
}

// BaseComponentStorage represents basic PVC configuration
type BaseComponentStorage interface {
	GetSize() string
	GetMountPath() string
	GetVolumeClaimTemplate() *corev1.PersistentVolumeClaim
}

// BaseComponentService represents basic service configuration
type BaseComponentService interface {
	GetPort() int32
	GetTargetPort() *int32
	GetPortName() string
	GetType() *corev1.ServiceType
	GetNodePort() *int32
	GetPorts() []corev1.ServicePort
	GetAnnotations() map[string]string
	GetProvides() ServiceBindingProvides
	GetConsumes() []ServiceBindingConsumes
	GetCertificate() Certificate
	GetCertificateSecretRef() *string
}

// BaseComponentMonitoring represents basic service monitoring configuration
type BaseComponentMonitoring interface {
	GetLabels() map[string]string
	GetEndpoints() []prometheusv1.Endpoint
}

// BaseComponentRoute represents route configuration
type BaseComponentRoute interface {
	GetCertificate() Certificate
	GetTermination() *routev1.TLSTerminationType
	GetInsecureEdgeTerminationPolicy() *routev1.InsecureEdgeTerminationPolicyType
	GetAnnotations() map[string]string
	GetHost() string
	GetPath() string
	GetCertificateSecretRef() *string
}

// ServiceBindingProvides represents a service to be provided
type ServiceBindingProvides interface {
	GetCategory() ServiceBindingCategory
	GetContext() string
	GetProtocol() string
	GetAuth() ServiceBindingAuth
}

// ServiceBindingConsumes represents a service to be consumed
type ServiceBindingConsumes interface {
	GetName() string
	GetNamespace() string
	GetCategory() ServiceBindingCategory
	GetMountPath() string
}

// ServiceBindingAuth represents authentication info when binding services
type ServiceBindingAuth interface {
	GetUsername() corev1.SecretKeySelector
	GetPassword() corev1.SecretKeySelector
}

// BaseComponentBindings represents Service Binding information
type BaseComponentBindings interface {
	GetAutoDetect() *bool
	GetResourceRef() string
	GetEmbedded() *runtime.RawExtension
}

// ServiceBindingCategory ...
type ServiceBindingCategory string

const (
	// ServiceBindingCategoryOpenAPI ...
	ServiceBindingCategoryOpenAPI ServiceBindingCategory = "openapi"
)

// BaseComponentAffinity describes deployment and pod affinity
type BaseComponentAffinity interface {
	GetNodeAffinity() *corev1.NodeAffinity
	GetPodAffinity() *corev1.PodAffinity
	GetPodAntiAffinity() *corev1.PodAntiAffinity
	GetArchitecture() []string
	GetNodeAffinityLabels() map[string]string
}

// BaseComponent represents basic kubernetes application
type BaseComponent interface {
	GetApplicationImage() string
	GetCommand() []string
	GetPullPolicy() *corev1.PullPolicy
	GetPullSecret() *string
	GetServiceAccountName() *string
	GetReplicas() *int32
	GetLivenessProbe() *corev1.Probe
	GetReadinessProbe() *corev1.Probe
	GetVolumes() []corev1.Volume
	GetVolumeMounts() []corev1.VolumeMount
	GetResourceConstraints() *corev1.ResourceRequirements
	GetExpose() *bool
	GetEnv() []corev1.EnvVar
	GetEnvFrom() []corev1.EnvFromSource
	GetCreateKnativeService() *bool
	GetArchitecture() []string
	GetAutoscaling() BaseComponentAutoscaling
	GetStorage() BaseComponentStorage
	GetService() BaseComponentService
	GetVersion() string
	GetCreateAppDefinition() *bool
	GetApplicationName() string
	GetMonitoring() BaseComponentMonitoring
	GetLabels() map[string]string
	GetAnnotations() map[string]string
	GetStatus() BaseComponentStatus
	GetInitContainers() []corev1.Container
	GetSidecarContainers() []corev1.Container
	GetGroupName() string
	GetRoute() BaseComponentRoute
	GetBindings() BaseComponentBindings
	GetAffinity() BaseComponentAffinity
}

// Certificate returns cert-manager CertificateSpec
type Certificate interface {
	GetSpec() certmngrv1alpha2.CertificateSpec
}
