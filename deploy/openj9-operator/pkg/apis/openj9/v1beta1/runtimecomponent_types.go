package v1beta1

import (
	"time"

	prometheusv1 "github.com/coreos/prometheus-operator/pkg/apis/monitoring/v1"
	"github.com/eclipse/openj9/pkg/common"
	routev1 "github.com/openshift/api/route/v1"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/runtime"
)

// NOTE: json tags are required.  Any new fields you add must have json tags for the fields to be serialized.

// RuntimeComponentSpec defines the desired state of RuntimeComponent
// +k8s:openapi-gen=true
type RuntimeComponentSpec struct {
	Version          string                       `json:"version,omitempty"`
	ApplicationImage string                       `json:"applicationImage"`
	Command          []string                     `json:"command,omitempty"`
	Replicas         *int32                       `json:"replicas,omitempty"`
	Autoscaling      *RuntimeComponentAutoScaling `json:"autoscaling,omitempty"`
	PullPolicy       *corev1.PullPolicy           `json:"pullPolicy,omitempty"`
	PullSecret       *string                      `json:"pullSecret,omitempty"`
	// +listType=map
	// +listMapKey=name
	Volumes []corev1.Volume `json:"volumes,omitempty"`
	// +listType=atomic
	VolumeMounts        []corev1.VolumeMount         `json:"volumeMounts,omitempty"`
	ResourceConstraints *corev1.ResourceRequirements `json:"resourceConstraints,omitempty"`
	ReadinessProbe      *corev1.Probe                `json:"readinessProbe,omitempty"`
	LivenessProbe       *corev1.Probe                `json:"livenessProbe,omitempty"`
	Service             *RuntimeComponentService     `json:"service,omitempty"`
	Expose              *bool                        `json:"expose,omitempty"`
	// +listType=atomic
	EnvFrom []corev1.EnvFromSource `json:"envFrom,omitempty"`
	// +listType=map
	// +listMapKey=name
	Env                []corev1.EnvVar `json:"env,omitempty"`
	ServiceAccountName *string         `json:"serviceAccountName,omitempty"`
	// +listType=set
	Architecture         []string                    `json:"architecture,omitempty"`
	Storage              *RuntimeComponentStorage    `json:"storage,omitempty"`
	CreateKnativeService *bool                       `json:"createKnativeService,omitempty"`
	Monitoring           *RuntimeComponentMonitoring `json:"monitoring,omitempty"`
	CreateAppDefinition  *bool                       `json:"createAppDefinition,omitempty"`
	ApplicationName      string                      `json:"applicationName,omitempty"`
	// +listType=map
	// +listMapKey=name
	InitContainers []corev1.Container `json:"initContainers,omitempty"`
	// +listType=map
	// +listMapKey=name
	SidecarContainers []corev1.Container        `json:"sidecarContainers,omitempty"`
	Route             *RuntimeComponentRoute    `json:"route,omitempty"`
	Bindings          *RuntimeComponentBindings `json:"bindings,omitempty"`
	Affinity          *RuntimeComponentAffinity `json:"affinity,omitempty"`
}

// RuntimeComponentAffinity deployment affinity settings
// +k8s:openapi-gen=true
type RuntimeComponentAffinity struct {
	NodeAffinity    *corev1.NodeAffinity    `json:"nodeAffinity,omitempty"`
	PodAffinity     *corev1.PodAffinity     `json:"podAffinity,omitempty"`
	PodAntiAffinity *corev1.PodAntiAffinity `json:"podAntiAffinity,omitempty"`
	// +listType=set
	Architecture       []string          `json:"architecture,omitempty"`
	NodeAffinityLabels map[string]string `json:"nodeAffinityLabels,omitempty"`
}

// RuntimeComponentAutoScaling ...
// +k8s:openapi-gen=true
type RuntimeComponentAutoScaling struct {
	TargetCPUUtilizationPercentage *int32 `json:"targetCPUUtilizationPercentage,omitempty"`
	MinReplicas                    *int32 `json:"minReplicas,omitempty"`

	// +kubebuilder:validation:Minimum=1
	MaxReplicas int32 `json:"maxReplicas,omitempty"`
}

// RuntimeComponentService ...
// +k8s:openapi-gen=true
type RuntimeComponentService struct {
	Type *corev1.ServiceType `json:"type,omitempty"`

	// +kubebuilder:validation:Maximum=65535
	// +kubebuilder:validation:Minimum=1
	Port int32 `json:"port,omitempty"`
	// +kubebuilder:validation:Maximum=65535
	// +kubebuilder:validation:Minimum=1
	TargetPort *int32 `json:"targetPort,omitempty"`

	// +kubebuilder:validation:Maximum=65535
	// +kubebuilder:validation:Minimum=0
	NodePort *int32 `json:"nodePort,omitempty"`

	PortName string `json:"portName,omitempty"`

	Ports []corev1.ServicePort `json:"ports,omitempty"`

	Annotations map[string]string `json:"annotations,omitempty"`
	// +listType=atomic
	Consumes []ServiceBindingConsumes `json:"consumes,omitempty"`
	Provides *ServiceBindingProvides  `json:"provides,omitempty"`
	// +k8s:openapi-gen=true
	Certificate          *Certificate `json:"certificate,omitempty"`
	CertificateSecretRef *string      `json:"certificateSecretRef,omitempty"`
}

// ServiceBindingProvides represents information about
// +k8s:openapi-gen=true
type ServiceBindingProvides struct {
	Category common.ServiceBindingCategory `json:"category"`
	Context  string                        `json:"context,omitempty"`
	Protocol string                        `json:"protocol,omitempty"`
	Auth     *ServiceBindingAuth           `json:"auth,omitempty"`
}

// ServiceBindingConsumes represents a service to be consumed
// +k8s:openapi-gen=true
type ServiceBindingConsumes struct {
	Name      string                        `json:"name"`
	Namespace string                        `json:"namespace,omitempty"`
	Category  common.ServiceBindingCategory `json:"category"`
	MountPath string                        `json:"mountPath,omitempty"`
}

// RuntimeComponentStorage ...
// +k8s:openapi-gen=false
type RuntimeComponentStorage struct {
	// +kubebuilder:validation:Pattern=^([+-]?[0-9.]+)([eEinumkKMGTP]*[-+]?[0-9]*)$
	Size                string                        `json:"size,omitempty"`
	MountPath           string                        `json:"mountPath,omitempty"`
	VolumeClaimTemplate *corev1.PersistentVolumeClaim `json:"volumeClaimTemplate,omitempty"`
}

// RuntimeComponentMonitoring ...
type RuntimeComponentMonitoring struct {
	Labels    map[string]string       `json:"labels,omitempty"`
	Endpoints []prometheusv1.Endpoint `json:"endpoints,omitempty"`
}

// RuntimeComponentRoute ...
// +k8s:openapi-gen=true
type RuntimeComponentRoute struct {
	Annotations                   map[string]string                          `json:"annotations,omitempty"`
	Termination                   *routev1.TLSTerminationType                `json:"termination,omitempty"`
	InsecureEdgeTerminationPolicy *routev1.InsecureEdgeTerminationPolicyType `json:"insecureEdgeTerminationPolicy,omitempty"`
	Certificate                   *Certificate                               `json:"certificate,omitempty"`
	CertificateSecretRef          *string                                    `json:"certificateSecretRef,omitempty"`
	Host                          string                                     `json:"host,omitempty"`
	Path                          string                                     `json:"path,omitempty"`
}

// ServiceBindingAuth allows a service to provide authentication information
type ServiceBindingAuth struct {
	// The secret that contains the username for authenticating
	Username corev1.SecretKeySelector `json:"username,omitempty"`
	// The secret that contains the password for authenticating
	Password corev1.SecretKeySelector `json:"password,omitempty"`
}

// RuntimeComponentBindings represents service binding related parameters
type RuntimeComponentBindings struct {
	AutoDetect  *bool                 `json:"autoDetect,omitempty"`
	ResourceRef string                `json:"resourceRef,omitempty"`
	Embedded    *runtime.RawExtension `json:"embedded,omitempty"`
}

// RuntimeComponentStatus defines the observed state of RuntimeComponent
// +k8s:openapi-gen=true
type RuntimeComponentStatus struct {
	// +listType=atomic
	Conditions       []StatusCondition       `json:"conditions,omitempty"`
	ConsumedServices common.ConsumedServices `json:"consumedServices,omitempty"`
	// +listType=set
	ResolvedBindings []string `json:"resolvedBindings,omitempty"`
	ImageReference   string   `json:"imageReference,omitempty"`
}

// StatusCondition ...
// +k8s:openapi-gen=true
type StatusCondition struct {
	LastTransitionTime *metav1.Time           `json:"lastTransitionTime,omitempty"`
	LastUpdateTime     metav1.Time            `json:"lastUpdateTime,omitempty"`
	Reason             string                 `json:"reason,omitempty"`
	Message            string                 `json:"message,omitempty"`
	Status             corev1.ConditionStatus `json:"status,omitempty"`
	Type               StatusConditionType    `json:"type,omitempty"`
}

// StatusConditionType ...
type StatusConditionType string

const (
	// StatusConditionTypeReconciled ...
	StatusConditionTypeReconciled StatusConditionType = "Reconciled"

	// StatusConditionTypeDependenciesSatisfied ...
	StatusConditionTypeDependenciesSatisfied StatusConditionType = "DependenciesSatisfied"
)

// +k8s:deepcopy-gen:interfaces=k8s.io/apimachinery/pkg/runtime.Object

// RuntimeComponent is the Schema for the runtimecomponents API
// +k8s:openapi-gen=true
// +kubebuilder:resource:path=runtimecomponents,scope=Namespaced,shortName=comp;comps
// +kubebuilder:subresource:status
// +kubebuilder:printcolumn:name="Image",type="string",JSONPath=".spec.applicationImage",priority=0,description="Absolute name of the deployed image containing registry and tag"
// +kubebuilder:printcolumn:name="Exposed",type="boolean",JSONPath=".spec.expose",priority=0,description="Specifies whether deployment is exposed externally via default Route"
// +kubebuilder:printcolumn:name="Reconciled",type="string",JSONPath=".status.conditions[?(@.type=='Reconciled')].status",priority=0,description="Status of the reconcile condition"
// +kubebuilder:printcolumn:name="Reason",type="string",JSONPath=".status.conditions[?(@.type=='Reconciled')].reason",priority=1,description="Reason for the failure of reconcile condition"
// +kubebuilder:printcolumn:name="Message",type="string",JSONPath=".status.conditions[?(@.type=='Reconciled')].message",priority=1,description="Failure message from reconcile condition"
// +kubebuilder:printcolumn:name="DependenciesSatisfied",type="string",JSONPath=".status.conditions[?(@.type=='DependenciesSatisfied')].status",priority=1,description="Status of the application dependencies"
// +kubebuilder:printcolumn:name="Age",type="date",JSONPath=".metadata.creationTimestamp",priority=0,description="Age of the resource"
type RuntimeComponent struct {
	metav1.TypeMeta   `json:",inline"`
	metav1.ObjectMeta `json:"metadata,omitempty"`

	Spec   RuntimeComponentSpec   `json:"spec,omitempty"`
	Status RuntimeComponentStatus `json:"status,omitempty"`
}

// +k8s:deepcopy-gen:interfaces=k8s.io/apimachinery/pkg/runtime.Object

// RuntimeComponentList contains a list of RuntimeComponent
type RuntimeComponentList struct {
	metav1.TypeMeta `json:",inline"`
	metav1.ListMeta `json:"metadata,omitempty"`
	Items           []RuntimeComponent `json:"items"`
}

func init() {
	SchemeBuilder.Register(&RuntimeComponent{}, &RuntimeComponentList{})
}

// GetApplicationImage returns application image
func (cr *RuntimeComponent) GetApplicationImage() string {
	return cr.Spec.ApplicationImage
}

// GetCommand returns command
func (cr *RuntimeComponent) GetCommand() []string {
	return cr.Spec.Command
}

// GetPullPolicy returns image pull policy
func (cr *RuntimeComponent) GetPullPolicy() *corev1.PullPolicy {
	return cr.Spec.PullPolicy
}

// GetPullSecret returns secret name for docker registry credentials
func (cr *RuntimeComponent) GetPullSecret() *string {
	return cr.Spec.PullSecret
}

// GetServiceAccountName returns service account name
func (cr *RuntimeComponent) GetServiceAccountName() *string {
	return cr.Spec.ServiceAccountName
}

// GetReplicas returns number of replicas
func (cr *RuntimeComponent) GetReplicas() *int32 {
	return cr.Spec.Replicas
}

// GetLivenessProbe returns liveness probe
func (cr *RuntimeComponent) GetLivenessProbe() *corev1.Probe {
	return cr.Spec.LivenessProbe
}

// GetReadinessProbe returns readiness probe
func (cr *RuntimeComponent) GetReadinessProbe() *corev1.Probe {
	return cr.Spec.ReadinessProbe
}

// GetVolumes returns volumes slice
func (cr *RuntimeComponent) GetVolumes() []corev1.Volume {
	return cr.Spec.Volumes
}

// GetVolumeMounts returns volume mounts slice
func (cr *RuntimeComponent) GetVolumeMounts() []corev1.VolumeMount {
	return cr.Spec.VolumeMounts
}

// GetResourceConstraints returns resource constraints
func (cr *RuntimeComponent) GetResourceConstraints() *corev1.ResourceRequirements {
	return cr.Spec.ResourceConstraints
}

// GetExpose returns expose flag
func (cr *RuntimeComponent) GetExpose() *bool {
	return cr.Spec.Expose
}

// GetEnv returns slice of environment variables
func (cr *RuntimeComponent) GetEnv() []corev1.EnvVar {
	return cr.Spec.Env
}

// GetEnvFrom returns slice of environment variables from source
func (cr *RuntimeComponent) GetEnvFrom() []corev1.EnvFromSource {
	return cr.Spec.EnvFrom
}

// GetCreateKnativeService returns flag that toggles Knative service
func (cr *RuntimeComponent) GetCreateKnativeService() *bool {
	return cr.Spec.CreateKnativeService
}

// GetArchitecture returns slice of architectures
func (cr *RuntimeComponent) GetArchitecture() []string {
	return cr.Spec.Architecture
}

// GetAutoscaling returns autoscaling settings
func (cr *RuntimeComponent) GetAutoscaling() common.BaseComponentAutoscaling {
	if cr.Spec.Autoscaling == nil {
		return nil
	}
	return cr.Spec.Autoscaling
}

// GetStorage returns storage settings
func (cr *RuntimeComponent) GetStorage() common.BaseComponentStorage {
	if cr.Spec.Storage == nil {
		return nil
	}
	return cr.Spec.Storage
}

// GetService returns service settings
func (cr *RuntimeComponent) GetService() common.BaseComponentService {
	if cr.Spec.Service == nil {
		return nil
	}
	return cr.Spec.Service
}

// GetVersion returns application version
func (cr *RuntimeComponent) GetVersion() string {
	return cr.Spec.Version
}

// GetCreateAppDefinition returns a toggle for integration with kAppNav
func (cr *RuntimeComponent) GetCreateAppDefinition() *bool {
	return cr.Spec.CreateAppDefinition
}

// GetApplicationName returns Application name to be used for integration with kAppNav
func (cr *RuntimeComponent) GetApplicationName() string {
	return cr.Spec.ApplicationName
}

// GetMonitoring returns monitoring settings
func (cr *RuntimeComponent) GetMonitoring() common.BaseComponentMonitoring {
	if cr.Spec.Monitoring == nil {
		return nil
	}
	return cr.Spec.Monitoring
}

// GetStatus returns RuntimeComponent status
func (cr *RuntimeComponent) GetStatus() common.BaseComponentStatus {
	return &cr.Status
}

// GetInitContainers returns list of init containers
func (cr *RuntimeComponent) GetInitContainers() []corev1.Container {
	return cr.Spec.InitContainers
}

// GetSidecarContainers returns list of user specified containers
func (cr *RuntimeComponent) GetSidecarContainers() []corev1.Container {
	return cr.Spec.SidecarContainers
}

// GetGroupName returns group name to be used in labels and annotation
func (cr *RuntimeComponent) GetGroupName() string {
	return "open.j9"
}

// GetRoute returns route configuration for RuntimeComponent
func (cr *RuntimeComponent) GetRoute() common.BaseComponentRoute {
	if cr.Spec.Route == nil {
		return nil
	}
	return cr.Spec.Route
}

// GetBindings returns route configuration for RuntimeComponent
func (cr *RuntimeComponent) GetBindings() common.BaseComponentBindings {
	if cr.Spec.Bindings == nil {
		return nil
	}
	return cr.Spec.Bindings
}

// GetAffinity returns deployment's node and pod affinity settings
func (cr *RuntimeComponent) GetAffinity() common.BaseComponentAffinity {
	if cr.Spec.Affinity == nil {
		return nil
	}
	return cr.Spec.Affinity
}

// GetResolvedBindings returns a map of all the service names to be consumed by the application
func (s *RuntimeComponentStatus) GetResolvedBindings() []string {
	return s.ResolvedBindings
}

// SetResolvedBindings sets ConsumedServices
func (s *RuntimeComponentStatus) SetResolvedBindings(rb []string) {
	s.ResolvedBindings = rb
}

// GetConsumedServices returns a map of all the service names to be consumed by the application
func (s *RuntimeComponentStatus) GetConsumedServices() common.ConsumedServices {
	if s.ConsumedServices == nil {
		return nil
	}
	return s.ConsumedServices
}

// SetConsumedServices sets ConsumedServices
func (s *RuntimeComponentStatus) SetConsumedServices(c common.ConsumedServices) {
	s.ConsumedServices = c
}

// GetImageReference returns Docker image reference to be deployed by the CR
func (s *RuntimeComponentStatus) GetImageReference() string {
	return s.ImageReference
}

// SetImageReference sets Docker image reference on the status portion of the CR
func (s *RuntimeComponentStatus) SetImageReference(imageReference string) {
	s.ImageReference = imageReference
}

// GetMinReplicas returns minimum replicas
func (a *RuntimeComponentAutoScaling) GetMinReplicas() *int32 {
	return a.MinReplicas
}

// GetMaxReplicas returns maximum replicas
func (a *RuntimeComponentAutoScaling) GetMaxReplicas() int32 {
	return a.MaxReplicas
}

// GetTargetCPUUtilizationPercentage returns target cpu usage
func (a *RuntimeComponentAutoScaling) GetTargetCPUUtilizationPercentage() *int32 {
	return a.TargetCPUUtilizationPercentage
}

// GetSize returns persistent volume size
func (s *RuntimeComponentStorage) GetSize() string {
	return s.Size
}

// GetMountPath returns mount path for persistent volume
func (s *RuntimeComponentStorage) GetMountPath() string {
	return s.MountPath
}

// GetVolumeClaimTemplate returns a template representing requested persistent volume
func (s *RuntimeComponentStorage) GetVolumeClaimTemplate() *corev1.PersistentVolumeClaim {
	return s.VolumeClaimTemplate
}

// GetAnnotations returns a set of annotations to be added to the service
func (s *RuntimeComponentService) GetAnnotations() map[string]string {
	return s.Annotations
}

// GetPort returns service port
func (s *RuntimeComponentService) GetPort() int32 {
	return s.Port
}

// GetNodePort returns service nodePort
func (s *RuntimeComponentService) GetNodePort() *int32 {
	if s.NodePort == nil {
		return nil
	}
	return s.NodePort
}

// GetTargetPort returns the internal target port for containers
func (s *RuntimeComponentService) GetTargetPort() *int32 {
	if s.TargetPort == nil {
		return nil
	}

	return s.TargetPort
}

// GetPortName returns name of service port
func (s *RuntimeComponentService) GetPortName() string {
	return s.PortName
}

// GetType returns service type
func (s *RuntimeComponentService) GetType() *corev1.ServiceType {
	return s.Type
}

// GetPorts returns a list of service ports
func (s *RuntimeComponentService) GetPorts() []corev1.ServicePort {
	return s.Ports
}

// GetProvides returns service provider configuration
func (s *RuntimeComponentService) GetProvides() common.ServiceBindingProvides {
	if s.Provides == nil {
		return nil
	}
	return s.Provides
}

// GetCertificate returns services certificate configuration
func (s *RuntimeComponentService) GetCertificate() common.Certificate {
	if s.Certificate == nil {
		return nil
	}
	return s.Certificate
}

// GetCertificateSecretRef returns a secret reference with a certificate
func (s *RuntimeComponentService) GetCertificateSecretRef() *string {
	return s.CertificateSecretRef
}

// GetCategory returns category of a service provider configuration
func (p *ServiceBindingProvides) GetCategory() common.ServiceBindingCategory {
	return p.Category
}

// GetContext returns context of a service provider configuration
func (p *ServiceBindingProvides) GetContext() string {
	return p.Context
}

// GetAuth returns secret of a service provider configuration
func (p *ServiceBindingProvides) GetAuth() common.ServiceBindingAuth {
	if p.Auth == nil {
		return nil
	}
	return p.Auth
}

// GetProtocol returns protocol of a service provider configuration
func (p *ServiceBindingProvides) GetProtocol() string {
	return p.Protocol
}

// GetConsumes returns a list of service consumers' configuration
func (s *RuntimeComponentService) GetConsumes() []common.ServiceBindingConsumes {
	consumes := make([]common.ServiceBindingConsumes, len(s.Consumes))
	for i := range s.Consumes {
		consumes[i] = &s.Consumes[i]
	}
	return consumes
}

// GetName returns service name of a service consumer configuration
func (c *ServiceBindingConsumes) GetName() string {
	return c.Name
}

// GetNamespace returns namespace of a service consumer configuration
func (c *ServiceBindingConsumes) GetNamespace() string {
	return c.Namespace
}

// GetCategory returns category of a service consumer configuration
func (c *ServiceBindingConsumes) GetCategory() common.ServiceBindingCategory {
	return common.ServiceBindingCategoryOpenAPI
}

// GetMountPath returns mount path of a service consumer configuration
func (c *ServiceBindingConsumes) GetMountPath() string {
	return c.MountPath
}

// GetUsername returns username of a service binding auth object
func (a *ServiceBindingAuth) GetUsername() corev1.SecretKeySelector {
	return a.Username
}

// GetPassword returns password of a service binding auth object
func (a *ServiceBindingAuth) GetPassword() corev1.SecretKeySelector {
	return a.Password
}

// GetLabels returns labels to be added on ServiceMonitor
func (m *RuntimeComponentMonitoring) GetLabels() map[string]string {
	return m.Labels
}

// GetEndpoints returns endpoints to be added to ServiceMonitor
func (m *RuntimeComponentMonitoring) GetEndpoints() []prometheusv1.Endpoint {
	return m.Endpoints
}

// GetAnnotations returns route annotations
func (r *RuntimeComponentRoute) GetAnnotations() map[string]string {
	return r.Annotations
}

// GetCertificate returns certificate spec for route
func (r *RuntimeComponentRoute) GetCertificate() common.Certificate {
	if r.Certificate == nil {
		return nil
	}
	return r.Certificate
}

// GetCertificateSecretRef returns a secret reference with a certificate
func (r *RuntimeComponentRoute) GetCertificateSecretRef() *string {
	return r.CertificateSecretRef
}

// GetTermination returns terminatation of the route's TLS
func (r *RuntimeComponentRoute) GetTermination() *routev1.TLSTerminationType {
	return r.Termination
}

// GetInsecureEdgeTerminationPolicy returns terminatation of the route's TLS
func (r *RuntimeComponentRoute) GetInsecureEdgeTerminationPolicy() *routev1.InsecureEdgeTerminationPolicyType {
	return r.InsecureEdgeTerminationPolicy
}

// GetHost returns hostname to be used by the route
func (r *RuntimeComponentRoute) GetHost() string {
	return r.Host
}

// GetPath returns path to use for the route
func (r *RuntimeComponentRoute) GetPath() string {
	return r.Path
}

// GetAutoDetect returns a boolean to specify if the operator should auto-detect ServiceBinding CRs with the same name as the RuntimeComponent CR
func (r *RuntimeComponentBindings) GetAutoDetect() *bool {
	return r.AutoDetect
}

// GetResourceRef returns name of ServiceBinding CRs created manually in the same namespace as the RuntimeComponent CR
func (r *RuntimeComponentBindings) GetResourceRef() string {
	return r.ResourceRef
}

// GetEmbedded returns the embedded underlying Service Binding resource
func (r *RuntimeComponentBindings) GetEmbedded() *runtime.RawExtension {
	return r.Embedded
}

// GetNodeAffinity returns node affinity
func (a *RuntimeComponentAffinity) GetNodeAffinity() *corev1.NodeAffinity {
	return a.NodeAffinity
}

// GetPodAffinity returns pod affinity
func (a *RuntimeComponentAffinity) GetPodAffinity() *corev1.PodAffinity {
	return a.PodAffinity
}

// GetPodAntiAffinity returns pod anti-affinity
func (a *RuntimeComponentAffinity) GetPodAntiAffinity() *corev1.PodAntiAffinity {
	return a.PodAntiAffinity
}

// GetArchitecture returns list of architecture names
func (a *RuntimeComponentAffinity) GetArchitecture() []string {
	return a.Architecture
}

// GetNodeAffinityLabels returns list of architecture names
func (a *RuntimeComponentAffinity) GetNodeAffinityLabels() map[string]string {
	return a.NodeAffinityLabels
}

// Initialize the RuntimeComponent instance
func (cr *RuntimeComponent) Initialize() {
	if cr.Spec.PullPolicy == nil {
		pp := corev1.PullIfNotPresent
		cr.Spec.PullPolicy = &pp
	}

	if cr.Spec.ResourceConstraints == nil {
		cr.Spec.ResourceConstraints = &corev1.ResourceRequirements{}
	}

	// Default applicationName to cr.Name, if a user sets createAppDefinition to true but doesn't set applicationName
	if cr.Spec.ApplicationName == "" {
		if cr.Labels != nil && cr.Labels["app.kubernetes.io/part-of"] != "" {
			cr.Spec.ApplicationName = cr.Labels["app.kubernetes.io/part-of"]
		} else {
			cr.Spec.ApplicationName = cr.Name
		}
	}

	if cr.Labels != nil {
		cr.Labels["app.kubernetes.io/part-of"] = cr.Spec.ApplicationName
	}

	// This is to handle when there is no service in the CR
	if cr.Spec.Service == nil {
		cr.Spec.Service = &RuntimeComponentService{}
	}

	if cr.Spec.Service.Type == nil {
		st := corev1.ServiceTypeClusterIP
		cr.Spec.Service.Type = &st
	}

	if cr.Spec.Service.Port == 0 {
		cr.Spec.Service.Port = 8080
	}

	if cr.Spec.Service.Provides != nil && cr.Spec.Service.Provides.Protocol == "" {
		cr.Spec.Service.Provides.Protocol = "http"
	}

	if cr.Spec.Service.Certificate != nil {
		if cr.Spec.Service.Certificate.IssuerRef.Name == "" {
			cr.Spec.Service.Certificate.IssuerRef.Name = common.Config[common.OpConfigPropDefaultIssuer]
		}

		if cr.Spec.Service.Certificate.IssuerRef.Kind == "" && common.Config[common.OpConfigPropUseClusterIssuer] != "false" {
			cr.Spec.Service.Certificate.IssuerRef.Kind = "ClusterIssuer"
		}
	}

	if cr.Spec.Route != nil && cr.Spec.Route.Certificate != nil {
		if cr.Spec.Route.Certificate.IssuerRef.Name == "" {
			cr.Spec.Route.Certificate.IssuerRef.Name = common.Config[common.OpConfigPropDefaultIssuer]
		}

		if cr.Spec.Route.Certificate.IssuerRef.Kind == "" && common.Config[common.OpConfigPropUseClusterIssuer] != "false" {
			cr.Spec.Route.Certificate.IssuerRef.Kind = "ClusterIssuer"
		}
	}
}

// GetLabels returns set of labels to be added to all resources
func (cr *RuntimeComponent) GetLabels() map[string]string {
	labels := map[string]string{
		"app.kubernetes.io/instance":   cr.Name,
		"app.kubernetes.io/name":       cr.Name,
		"app.kubernetes.io/managed-by": "openj9-operator",
		"app.kubernetes.io/component":  "backend",
		"app.kubernetes.io/part-of":    cr.Spec.ApplicationName,
	}

	if cr.Spec.Version != "" {
		labels["app.kubernetes.io/version"] = cr.Spec.Version
	}

	for key, value := range cr.Labels {
		if key != "app.kubernetes.io/instance" {
			labels[key] = value
		}
	}

	if cr.Spec.Service != nil && cr.Spec.Service.Provides != nil {
		labels["service.openj9/bindable"] = "true"
	}

	return labels
}

// GetAnnotations returns set of annotations to be added to all resources
func (cr *RuntimeComponent) GetAnnotations() map[string]string {
	annotations := map[string]string{}
	for k, v := range cr.Annotations {
		annotations[k] = v
	}
	delete(annotations, "kubectl.kubernetes.io/last-applied-configuration")
	return annotations
}

// GetType returns status condition type
func (c *StatusCondition) GetType() common.StatusConditionType {
	return convertToCommonStatusConditionType(c.Type)
}

// SetType returns status condition type
func (c *StatusCondition) SetType(ct common.StatusConditionType) {
	c.Type = convertFromCommonStatusConditionType(ct)
}

// GetLastTransitionTime return time of last status change
func (c *StatusCondition) GetLastTransitionTime() *metav1.Time {
	return c.LastTransitionTime
}

// SetLastTransitionTime sets time of last status change
func (c *StatusCondition) SetLastTransitionTime(t *metav1.Time) {
	c.LastTransitionTime = t
}

// GetLastUpdateTime return time of last status update
func (c *StatusCondition) GetLastUpdateTime() metav1.Time {
	return c.LastUpdateTime
}

// SetLastUpdateTime sets time of last status update
func (c *StatusCondition) SetLastUpdateTime(t metav1.Time) {
	c.LastUpdateTime = t
}

// GetMessage return condition's message
func (c *StatusCondition) GetMessage() string {
	return c.Message
}

// SetMessage sets condition's message
func (c *StatusCondition) SetMessage(m string) {
	c.Message = m
}

// GetReason return condition's message
func (c *StatusCondition) GetReason() string {
	return c.Reason
}

// SetReason sets condition's reason
func (c *StatusCondition) SetReason(r string) {
	c.Reason = r
}

// GetStatus return condition's status
func (c *StatusCondition) GetStatus() corev1.ConditionStatus {
	return c.Status
}

// SetStatus sets condition's status
func (c *StatusCondition) SetStatus(s corev1.ConditionStatus) {
	c.Status = s
}

// NewCondition returns new condition
func (s *RuntimeComponentStatus) NewCondition() common.StatusCondition {
	return &StatusCondition{}
}

// GetConditions returns slice of conditions
func (s *RuntimeComponentStatus) GetConditions() []common.StatusCondition {
	var conditions = make([]common.StatusCondition, len(s.Conditions))
	for i := range s.Conditions {
		conditions[i] = &s.Conditions[i]
	}
	return conditions
}

// GetCondition ...
func (s *RuntimeComponentStatus) GetCondition(t common.StatusConditionType) common.StatusCondition {
	for i := range s.Conditions {
		if s.Conditions[i].GetType() == t {
			return &s.Conditions[i]
		}
	}
	return nil
}

// SetCondition ...
func (s *RuntimeComponentStatus) SetCondition(c common.StatusCondition) {
	condition := &StatusCondition{}
	found := false
	for i := range s.Conditions {
		if s.Conditions[i].GetType() == c.GetType() {
			condition = &s.Conditions[i]
			found = true
		}
	}

	if condition.GetStatus() != c.GetStatus() {
		condition.SetLastTransitionTime(&metav1.Time{Time: time.Now()})
	}

	condition.SetLastUpdateTime(metav1.Time{Time: time.Now()})
	condition.SetReason(c.GetReason())
	condition.SetMessage(c.GetMessage())
	condition.SetStatus(c.GetStatus())
	condition.SetType(c.GetType())
	if !found {
		s.Conditions = append(s.Conditions, *condition)
	}
}

func convertToCommonStatusConditionType(c StatusConditionType) common.StatusConditionType {
	switch c {
	case StatusConditionTypeReconciled:
		return common.StatusConditionTypeReconciled
	case StatusConditionTypeDependenciesSatisfied:
		return common.StatusConditionTypeDependenciesSatisfied
	default:
		panic(c)
	}
}

func convertFromCommonStatusConditionType(c common.StatusConditionType) StatusConditionType {
	switch c {
	case common.StatusConditionTypeReconciled:
		return StatusConditionTypeReconciled
	case common.StatusConditionTypeDependenciesSatisfied:
		return StatusConditionTypeDependenciesSatisfied
	default:
		panic(c)
	}
}
